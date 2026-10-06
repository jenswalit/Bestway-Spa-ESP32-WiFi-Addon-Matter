#include "scheduler_manager.h"
#include "nvs.h"
#include <cassert>
#include <cstdlib>
#include <string>
#include <iostream>
struct httpd_req_t {std::string body,status="200 OK",response;};
constexpr int HTTPD_400_BAD_REQUEST=400,HTTPD_404_NOT_FOUND=404;
int read_request_body(httpd_req_t*r,std::string&b,int){b=r->body;return ESP_OK;}
std::string form_value(const std::string&s,const char*k){std::string key=std::string(k)+"=";size_t pos=0;while(pos<s.size()){auto e=s.find('&',pos);auto part=s.substr(pos,e-pos);if(part.find(key)==0)return part.substr(key.size());if(e==std::string::npos)break;pos=e+1;}return "";}
void httpd_resp_send_err(httpd_req_t*r,int n,const char*s){r->status=std::to_string(n);r->response=s;}
void httpd_resp_set_status(httpd_req_t*r,const char*s){r->status=s;}
void send_json(httpd_req_t*r,const std::string&s){r->response=s;}
bool parse_schedule_form(const std::string&,ScheduleItem&,bool,uint16_t*){return false;}
#include "handler.inc"
int main(){
 ScheduleItem x;x.days_mask=127;x.enabled=true;x.hour=13;x.minute=30;x.action=ScheduleAction::FilterOff;
 assert(scheduler_manager.reset());assert(scheduler_manager.add(x));int id=scheduler_manager.get(0)->id;
 httpd_req_t ok{"id="+std::to_string(id)+"&dayMask=2&enabled=false"};assert(api_schedules_put(&ok)==ESP_OK);assert(ok.status=="200 OK");assert(scheduler_manager.count()==2);assert(scheduler_manager.get(0)->enabled&&scheduler_manager.get(0)->days_mask==125);
 for(auto body:{"id=1&dayMask=3&enabled=false","id=1&dayMask=2&enabled=maybe","id=x&dayMask=2&enabled=false","id=1&dayMask=128&enabled=false"}){httpd_req_t bad{body};assert(api_schedules_put(&bad)==ESP_FAIL);assert(bad.status=="400");}
 httpd_req_t absent{"id=65535&dayMask=2&enabled=false"};assert(api_schedules_put(&absent)==ESP_OK);assert(absent.status=="404 Not Found");
 assert(scheduler_manager.reset());for(int i=0;i<24;++i){x.minute=i;assert(scheduler_manager.add(x));}id=scheduler_manager.get(0)->id;
 httpd_req_t full{"id="+std::to_string(id)+"&dayMask=2&enabled=false"};assert(api_schedules_put(&full)==ESP_OK);assert(full.status=="409 Conflict");assert(full.response.find("24")!=std::string::npos);
 std::cout<<"PASS: actual HTTP handler uses single day, rejects invalid requests, reports full planner\n";
}
