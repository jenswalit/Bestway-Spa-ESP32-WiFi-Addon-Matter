const fs=require('fs'),vm=require('vm'),assert=require('assert');
const code=fs.readFileSync('firmware/components/web_server/web/js/app.js','utf8');
const snippets=code.slice(code.indexOf('function plannerActionText('),code.indexOf('async function saveSchedule('))+code.slice(code.indexOf('let scheduleDayToggleBusy='),code.indexOf('async function deleteScheduleDay('));
const days=[['zo',1],['ma',2],['di',4],['wo',8],['do',16],['vr',32],['za',64]].map(([id,mask])=>({id,mask}));
const nodes=Object.fromEntries(days.map(d=>['day-'+d.id,{innerHTML:''}]));
let original={id:13,enabled:true,daysMask:127,hour:13,minute:30,action:3,value:0};let server=[original],calls=[],full=false;
const context={URLSearchParams,console,plannerDayDefinitions:days,plannerActionLabels:{3:'Filter uit'},schedulesData:[{...original}],tr:x=>x,escapeHtml:x=>x,translateDom:()=>{},showToast:()=>{},document:{getElementById:id=>nodes[id]||{},querySelectorAll:()=>[]},fetch:async(url,options={})=>{calls.push([url,options]);if(options.method==='PUT'){let fields=new URLSearchParams(options.body);assert.equal(fields.get('id'),'13');assert.equal(fields.get('dayMask'),'2');assert.equal(fields.get('enabled'),'false');assert(!fields.has('daysMask'));if(full)return {ok:false,json:async()=>({error:'Planner vol'})};server=[{...original,daysMask:125},{...original,id:14,daysMask:2,enabled:false}];return {ok:true,json:async()=>({ok:true})};}return {ok:true,json:async()=>server.map(x=>({...x}))};}};
vm.createContext(context);vm.runInContext(snippets,context);
(async()=>{
 await vm.runInContext('setScheduleEnabled(13,2,false)',context);
 assert.equal(calls.length,2);assert(nodes['day-ma'].innerHTML.includes('setScheduleEnabled(14,2,this.checked)'));assert(!nodes['day-ma'].innerHTML.includes('checked onchange'));assert(nodes['day-di'].innerHTML.includes('checked onchange'));assert(context.schedulesData[0].enabled);
 full=true;calls=[];server=[{...original}];context.schedulesData=[{...original}];await vm.runInContext('setScheduleEnabled(13,2,false)',context);assert.equal(calls.length,2);assert(nodes['day-ma'].innerHTML.includes('checked onchange'));assert(nodes['day-di'].innerHTML.includes('checked onchange'));
 console.log('PASS: real web functions pass dayMask, reload server state, show only Monday off, restore on 409');
})().catch(e=>{console.error(e);process.exitCode=1;});


// Exercise the real capability function across simulator and hardware states.
{
 const ids=['power','controlPower','heater','controlHeater'];
 const elements=Object.fromEntries(ids.map(id=>[id,{disabled:false,title:'',removeAttribute(name){delete this[name];}}]));
 const targetButtons=[0,1].map(()=>({disabled:false,title:'',removeAttribute(name){delete this[name];}}));
 const ui={state:{},tr:x=>x,ensureGodModeControl:()=>null,ensureFullPowerControl:()=>null,setControlActive:()=>{},
   document:{getElementById:id=>elements[id],querySelectorAll:()=>targetButtons}};
 vm.createContext(ui);
 vm.runInContext(code.slice(code.indexOf('function applyProtocolCapabilities()'),code.indexOf('function resetLiveControls()')),ui);
 for(const protocolFamily of ['4wire','type1','type2']) {
   ui.state={busMode:'SIMULATION',protocolFamily,godMode:false};
   vm.runInContext('applyProtocolCapabilities()',ui);
   for(const button of [...Object.values(elements),...targetButtons]) {assert.equal(button.disabled,false);assert.equal(button.title,undefined);}
 }
 ui.state={busMode:'HARDWARE',protocolFamily:'4wire',godMode:false};
 vm.runInContext('applyProtocolCapabilities()',ui);
 for(const button of [...Object.values(elements),...targetButtons]) assert.equal(button.disabled,true);
 ui.state.godMode=true;
 vm.runInContext('applyProtocolCapabilities()',ui);
 assert.equal(elements.power.disabled,true);assert.equal(elements.heater.disabled,false);
 targetButtons.forEach(button=>assert.equal(button.disabled,false));
 ui.state={busMode:'HARDWARE',protocolFamily:'type2',godMode:false};
 vm.runInContext('applyProtocolCapabilities()',ui);
 assert.equal(elements.power.disabled,true);assert.equal(elements.heater.disabled,false);
 targetButtons.forEach(button=>assert.equal(button.disabled,false));
 ui.state={busMode:'HARDWARE',protocolFamily:'type1',godMode:false};
 vm.runInContext('applyProtocolCapabilities()',ui);
 for(const button of [...Object.values(elements),...targetButtons]) assert.equal(button.disabled,false);
 console.log('PASS: demo controls enabled for every protocol; hardware/Godmode restrictions preserved');
}
