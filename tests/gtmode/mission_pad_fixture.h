/* P: test-driver policy, not GT4 AI or a dynamics replacement. Only a pad
   command is produced from read-only course/car geometry. All gains,
   lookahead and speed choices below are explicit fixture input. */
#include <math.h>
static void mission_pad_fixture(gt4_race_run *run,uint32_t car,uint8_t command[32])
{
    float speed=GT4_F32(car+0x6c8),point[3];
    float look=15.f+fabsf(speed)*.5f;
    float distance=GT4_F32(car+0x5a8);
    if(!gt4_race_run_course_point(run,distance+look,point))return;
    const char *lane_env=getenv("GT4_MISSION_LANE");
    if(lane_env){float ahead[3],lane=(float)atof(lane_env);
        if(gt4_race_run_course_point(run,distance+look+5,ahead)){
            float x=ahead[0]-point[0],z=ahead[2]-point[2],length=sqrtf(x*x+z*z);
            if(length>0){point[0]+=z/length*lane;point[2]-=x/length*lane;}
        }
    }
    float dx=point[0]-GT4_F32(car+CAR_POSITION);
    float dz=point[2]+GT4_F32(car+CAR_POSITION+4);
    float fx=GT4_F32(car+CAR_MATRIX),fz=-GT4_F32(car+CAR_MATRIX+4);
    float angle=atan2f(fz*dx-fx*dz,fx*dx+fz*dz);
    float steer=fmaxf(-1.f,fminf(1.f,angle*.6f));
    int16_t axis=(int16_t)(steer*4096),brake=fabsf(angle)>.2f && speed>20.f,throttle=!brake;
    command[0]=1;command[1]=2;
    memcpy(command+4,&axis,2);memcpy(command+8,&throttle,2);memcpy(command+10,&brake,2);
}
