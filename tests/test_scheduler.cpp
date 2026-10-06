#include "scheduler_manager.h"
#include "nvs.h"
#include <cassert>
#include <iostream>
ScheduleItem week(bool enabled=true){ScheduleItem x;x.days_mask=127;x.enabled=enabled;x.hour=13;x.minute=30;x.action=ScheduleAction::FilterOff;return x;}
const ScheduleItem* find(const SchedulerManager&s,int id){for(int i=0;i<s.count();++i)if(s.get(i)->id==id)return s.get(i);return nullptr;}
void assert_days(const SchedulerManager&s,uint8_t expected){unsigned all=0,on=0;for(int i=0;i<s.count();++i){auto x=s.get(i);assert(!(all&x->days_mask));all|=x->days_mask;if(x->enabled)on|=x->days_mask;}assert(all==127);assert(on==expected);}
int main(){
 SchedulerManager s;assert(s.reset());assert(s.add(week()));auto id=s.get(0)->id;
 assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::Ok);assert(s.count()==2);assert(find(s,id)->enabled);assert(find(s,id)->days_mask==125);assert_days(s,125);
 auto off=s.get(1)->id;assert(!s.get(1)->enabled&&s.get(1)->days_mask==2);
 SchedulerManager loaded;assert(loaded.load());assert_days(loaded,125);
 // A second disabled day joins the disabled rule, with no third slot.
 assert(s.set_day_enabled(id,4,false)==ScheduleDayResult::Ok);assert(s.count()==2);assert_days(s,121);
 assert(s.set_day_enabled(off,2,true)==ScheduleDayResult::Ok);assert(s.count()==2);assert_days(s,123);
 assert(s.set_day_enabled(off,4,true)==ScheduleDayResult::Ok);assert(s.count()==1);assert(find(s,id));assert_days(s,127);
 // All seven weekdays can be disabled and re-enabled while retaining at most two records.
 for(unsigned bit=1;bit<=64;bit<<=1){int active=0;for(int i=0;i<s.count();++i)if(s.get(i)->enabled&&(s.get(i)->days_mask&bit))active=s.get(i)->id;assert(active);assert(s.set_day_enabled(active,bit,false)==ScheduleDayResult::Ok);assert(s.count()<=2);}
 assert_days(s,0);
 for(unsigned bit=1;bit<=64;bit<<=1){int inactive=0;for(int i=0;i<s.count();++i)if(!s.get(i)->enabled&&(s.get(i)->days_mask&bit))inactive=s.get(i)->id;assert(inactive);assert(s.set_day_enabled(inactive,bit,true)==ScheduleDayResult::Ok);assert(s.count()<=2);}
 assert_days(s,127);assert(s.count()==1);
 assert(s.set_day_enabled(s.get(0)->id,3,false)==ScheduleDayResult::InvalidDay);
 assert(s.set_day_enabled(65535,2,false)==ScheduleDayResult::NotFound);
 // No-op does not save or change revision. A commit error rolls RAM back.
 id=s.get(0)->id;auto revision=s.revision();fail_commit=true;
 assert(s.set_day_enabled(id,2,true)==ScheduleDayResult::Ok);assert(s.revision()==revision);
 assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::SaveFailed);assert(s.count()==1);assert_days(s,127);assert(s.revision()==revision);fail_commit=false;
 assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::Ok);assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::InvalidDay);
 // A full planner rejects a required split with no partial edit.
 assert(s.reset());for(int i=0;i<24;++i){auto x=week();x.minute=i;assert(s.add(x));}id=s.get(0)->id;revision=s.revision();
 assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::Full);assert(s.count()==24);assert(find(s,id)->days_mask==127&&find(s,id)->enabled);assert(s.revision()==revision);
 // A one-day rule still works at capacity.
 auto one=*s.get(0);one.days_mask=2;assert(s.update(id,one));assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::Ok);assert(!find(s,id)->enabled);
 // Do not merge overlapping or different action/time records.
 assert(s.reset());assert(s.add(week()));auto other=week(false);assert(s.add(other));id=s.get(0)->id;assert(s.set_day_enabled(id,2,false)==ScheduleDayResult::Ok);assert(s.count()==3);
 std::cout<<"PASS: week split, merge, every day, persistence, invalid input, rollback, 24-rule limit, overlap protection\n";
}
