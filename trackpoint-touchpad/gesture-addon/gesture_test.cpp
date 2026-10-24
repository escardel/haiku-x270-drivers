#include "movement_maker.h"
#include <cassert>
#include <cstdio>
static touchpad_specs specs={50,4100,1432,5532,1368,4508,30,100,200};
static touchpad_settings settings=kDefaultTouchpadSettings;
static int checks=0;
#define CHECK(x) do { if(!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } checks++; } while(0)
static touchpad_movement event(int fingers,int x=3000,int y=3000,int z=30,int buttons=0) {
 touchpad_movement e={}; e.fingers=(1<<fingers)-1; e.xPosition=x; e.yPosition=y;
 e.zPressure=z; e.fingerWidth=4; e.buttons=buttons; return e;
}
static mouse_movement feed(TouchpadMovement& t,touchpad_movement e,bigtime_t* timeout=nullptr) {
 mouse_movement m={}; bigtime_t wait=0; assert(t.EventToMovement(&e,&m,wait)==B_OK);
 if(timeout) *timeout=wait; return m;
}
static void init(TouchpadMovement& t) { t.SetSpecs(specs); t.SetSettings(settings); }
int main() {
 settings.scroll_rightrange=0; settings.scroll_bottomrange=0;
 settings.finger_click=false; settings.edge_motion=0;
 { TouchpadMovement t; init(t); feed(t,event(1));
   auto m=feed(t,event(0,0,0,0)); CHECK(m.buttons==1); CHECK(m.clicks==1);
   snooze(210000); CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(1)); feed(t,event(3));
   feed(t,event(1,4200,2800)); bigtime_t wait;
   auto m=feed(t,event(0,0,0,0),&wait);
   CHECK(m.buttons==4); CHECK(wait==1000);
   CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(3));
   CHECK(feed(t,event(0,0,0,0)).buttons==4);
   CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(2));
   CHECK(feed(t,event(0,0,0,0)).buttons==2);
   CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(1)); feed(t,event(2,3800,3000));
   auto m=feed(t,event(2,3800,2300)); CHECK(m.wheel_ydelta!=0);
   CHECK(m.xdelta==0 && m.ydelta==0);
   m=feed(t,event(1,1900,4200)); CHECK(m.xdelta==0 && m.ydelta==0);
   m=feed(t,event(1,2200,4000)); CHECK(m.xdelta==0 && m.ydelta==0);
   CHECK(feed(t,event(0,0,0,0)).buttons==0);
   feed(t,event(1)); m=feed(t,event(1,3300)); CHECK(m.xdelta>0); }
 { TouchpadMovement t; init(t); feed(t,event(1,3000,3000,50,1));
   auto m=feed(t,event(1,3300,3000,50,1)); CHECK(m.buttons==1); CHECK(m.xdelta>0);
   CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); auto off=settings; off.tapgesture_sensibility=0; t.SetSettings(off);
   feed(t,event(1)); CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(1)); feed(t,event(1,3600));
   feed(t,event(1,3000)); CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 { TouchpadMovement t; init(t); feed(t,event(1)); snooze(210000);
   CHECK(feed(t,event(0,0,0,0)).buttons==0); }
 printf("PASS: %d gesture checks (light tap, three-finger middle, releases, scrolling lift-off, drag, disabled/long/moving taps)\n",checks);
}