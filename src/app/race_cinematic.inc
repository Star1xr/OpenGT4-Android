/* P: user-operated filming tool. All distances, input sensitivity and lens
   presets here are PC choices, never original race/physics parameters. */
static void cinematic_angles(race_game *g,const float rows[9])
{
    g->cinema.yaw=atan2f(-rows[6],-rows[8]);
    g->cinema.pitch=asinf(fmaxf(-1,fminf(1,-rows[7])));
    float sy=sinf(g->cinema.yaw),cy=cosf(g->cinema.yaw),sp=sinf(g->cinema.pitch);
    float cp=cosf(g->cinema.pitch),right[3]={-cy,0,sy},up[3]={-sy*sp,cp,-cy*sp};
    float a=0,b=0;for(int k=0;k<3;k++){a+=rows[k]*up[k];b+=rows[k]*right[k];}
    g->cinema.roll=atan2f(a,b);
}
static int cinematic_pose(race_game *g,float pos[3],float rot[9])
{
    float wheels[4][3];return gt4_race_run_pose(g->run,g->cinema.target,pos,rot,wheels);
}
static void cinematic_mode(race_game *g,int mode)
{
    float pos[3],rot[9],local[9];if(!cinematic_pose(g,pos,rot))return;
    /* Rebase from the currently visible world pose: no snap on mounting. */
    memcpy(g->cinema.anchor,pos,sizeof pos);
    if(mode==2){
        for(int j=0;j<3;j++){
            g->cinema.mount[j]=0;
            for(int k=0;k<3;k++)g->cinema.mount[j]+=rot[k*3+j]*(g->cinema.eye[k]-pos[k]);
        }
        for(int a=0;a<3;a++)for(int j=0;j<3;j++){
            local[a*3+j]=0;for(int k=0;k<3;k++)local[a*3+j]+=g->cinema.rows[a*3+k]*rot[k*3+j];
        }
        cinematic_angles(g,local);g->cinema.aim=0;
    }else cinematic_angles(g,g->cinema.rows);
    memset(g->cinema.velocity,0,sizeof g->cinema.velocity);g->cinema.mode=mode;
    if(mode==1)g->cinema.aim=1;
}
int race_game_cinematic(race_game *g){return g && g->cinema.on;}
void race_game_cinematic_toggle(race_game *g)
{
    if(!g)return;
    if(g->cinema.on){
        if(g->cinema.paused)g->held=g->cinema.was_held;
        race_view_set_camera(g->view,g->cinema.original_eye,g->cinema.original_rows,
                             g->cinema.original_projection[0],g->cinema.original_projection[1],g->cinema.original_projection[2]);
        race_view_show_car(g->view,g->cinema.original_visible);
        g->cinema.on=g->cinema.seeded=g->cinema.paused=0;
        SDL_SetRelativeMouseMode(SDL_FALSE);return;
    }
    if(!g->ready || g->split)return;
    if(!race_view_get_camera(g->view,g->cinema.eye,g->cinema.rows,g->cinema.projection))return;
    memcpy(g->cinema.original_eye,g->cinema.eye,sizeof g->cinema.eye);
    memcpy(g->cinema.original_rows,g->cinema.rows,sizeof g->cinema.rows);
    memcpy(g->cinema.original_projection,g->cinema.projection,sizeof g->cinema.projection);
    g->cinema.original_visible=race_view_car_visible(g->view);
    g->inspect_on=g->inspect_seeded=0;
    g->cinema.on=g->cinema.seeded=g->cinema.clean=1;
    g->cinema.target=watched_car(g);g->cinema.mode=g->cinema.aim=g->cinema.aperture=0;
    g->cinema.mouse[0]=g->cinema.mouse[1]=0;g->cinema.focus=10;g->cinema.focus_auto=1;
    cinematic_angles(g,g->cinema.rows);cinematic_mode(g,0);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    fprintf(stderr,"F9 cinema: WASD/QE move, mouse look, wheel lens, F follow, G mount, T aim, Tab car, B depth of field, Z/C roll, P hold, H HUD, Home reset\n");
}
int race_game_cinematic_key(race_game *g,int key)
{
    if(!race_game_cinematic(g))return 0;
    if(key==SDLK_ESCAPE){race_game_cinematic_toggle(g);return 1;}
    if(key==SDLK_f)cinematic_mode(g,g->cinema.mode==1?0:1);
    else if(key==SDLK_g)cinematic_mode(g,g->cinema.mode==2?0:2);
    else if(key==SDLK_t){if(g->cinema.mode==2)cinematic_mode(g,1);else g->cinema.aim=!g->cinema.aim;}
    else if(key==SDLK_h)g->cinema.clean=!g->cinema.clean;
    else if(key==SDLK_b)g->cinema.aperture=(g->cinema.aperture+1)%4;
    else if(key==SDLK_LEFTBRACKET || key==SDLK_RIGHTBRACKET){
        g->cinema.focus_auto=0;g->cinema.focus=fminf(20000,fmaxf(.2f,g->cinema.focus*(key==SDLK_LEFTBRACKET?.9f:1.1f)));
    }else if(key==SDLK_r)g->cinema.focus_auto=1;
    else if(key==SDLK_p){
        if(!g->cinema.paused){g->cinema.was_held=g->held;g->held=1;g->cinema.paused=1;}
        else{g->held=g->cinema.was_held;g->cinema.paused=0;}
    }else if(key==SDLK_TAB){
        float old[3],rot[9],next[3];int n=gt4_race_run_cars(g->run);
        if(n>0 && cinematic_pose(g,old,rot)){
            g->cinema.target=(g->cinema.target+1)%n;
            if(cinematic_pose(g,next,rot)){
                for(int k=0;k<3;k++)g->cinema.eye[k]+=next[k]-old[k];
                memcpy(g->cinema.anchor,next,sizeof next);
            }
        }
    }else if(key==SDLK_HOME){
        /* Read the selected car's existing chase-camera point, also while held. */
        float point[8];
        if(race_view_camera_point_entry(g->view,g->cinema.target,1,point) &&
           gt4_race_run_camera(g->run,g->cinema.target,1,point,g->cinema.eye,g->cinema.rows)){
            cinematic_angles(g,g->cinema.rows);g->cinema.seeded=1;cinematic_mode(g,0);
            g->cinema.projection[0]=1.73205f;g->cinema.projection[1]=2.25378f;
        }
        g->cinema.mode=g->cinema.aim=0;
        g->cinema.mouse[0]=g->cinema.mouse[1]=0;memset(g->cinema.velocity,0,sizeof g->cinema.velocity);
    }
    /* Camera keys never reach replay mode, pause menus or the driving pad. */
    return 1;
}
void race_game_cinematic_mouse(race_game *g,int dx,int dy,int wheel)
{
    if(!race_game_cinematic(g))return;
    if(dx || dy){
        g->cinema.mouse[0]-=dx*.0025f;g->cinema.mouse[1]-=dy*.0025f;
        if(g->cinema.mode!=1)g->cinema.aim=0;
    }
    float sx=g->cinema.projection[0],ratio=g->cinema.projection[1]/sx;
    sx=fminf(11.43f,fmaxf(.268f,sx*expf(fmaxf(-20,fminf(20,(float)wheel))*.09f)));
    g->cinema.projection[0]=sx;g->cinema.projection[1]=sx*ratio;
}
static void cinematic_camera(race_game *g,double dt)
{
    if(!g->cinema.on)return;
    float pos[3],rot[9];if(!cinematic_pose(g,pos,rot))return;
    if(!g->cinema.seeded){
        if(!race_view_get_camera(g->view,g->cinema.eye,g->cinema.rows,g->cinema.projection))return;
        cinematic_angles(g,g->cinema.rows);memcpy(g->cinema.anchor,pos,sizeof pos);g->cinema.seeded=1;
    }
    const Uint8 *keys=SDL_GetKeyboardState(NULL);
    if(!(SDL_GetWindowFlags(plat_sdl_window())&SDL_WINDOW_INPUT_FOCUS)){
        static const Uint8 empty[SDL_NUM_SCANCODES];keys=empty;
        g->cinema.mouse[0]=g->cinema.mouse[1]=0;
        memset(g->cinema.velocity,0,sizeof g->cinema.velocity);
    }
    float step=(float)fmax(0,fmin(dt,.1)),ease=1-expf(-12*step);
    float mx=g->cinema.mouse[0]*ease,my=g->cinema.mouse[1]*ease;
    if(g->cinema.mode==1)for(int k=0;k<3;k++)g->cinema.eye[k]+=pos[k]-g->cinema.anchor[k];
    if(g->cinema.mode==1 && g->cinema.aim){
        float x=g->cinema.eye[0]-pos[0],y=g->cinema.eye[1]-pos[1]-.65f,z=g->cinema.eye[2]-pos[2];
        float radius=fmaxf(.3f,sqrtf(x*x+y*y+z*z)),yaw=atan2f(x,z)+mx;
        float pitch=fmaxf(-1.52f,fminf(1.52f,atan2f(y,sqrtf(x*x+z*z))+my));
        g->cinema.eye[0]=pos[0]+radius*cosf(pitch)*sinf(yaw);
        g->cinema.eye[1]=pos[1]+.65f+radius*sinf(pitch);
        g->cinema.eye[2]=pos[2]+radius*cosf(pitch)*cosf(yaw);
    }else{g->cinema.yaw+=mx;g->cinema.pitch+=my;}
    g->cinema.mouse[0]*=1-ease;g->cinema.mouse[1]*=1-ease;
    g->cinema.pitch=fmaxf(-1.55f,fminf(1.55f,g->cinema.pitch));
    g->cinema.roll+=step*.6f*(keys[SDL_SCANCODE_C]-keys[SDL_SCANCODE_Z]);
    float sy=sinf(g->cinema.yaw),cy=cosf(g->cinema.yaw),sp=sinf(g->cinema.pitch),cp=cosf(g->cinema.pitch);
    float rows[9]={-cy,0,sy,-sy*sp,cp,-cy*sp,-sy*cp,-sp,-cy*cp};
    float cr=cosf(g->cinema.roll),sr=sinf(g->cinema.roll);
    for(int k=0;k<3;k++){float r=rows[k],u=rows[3+k];rows[k]=r*cr+u*sr;rows[3+k]=u*cr-r*sr;}
    float speed=(keys[SDL_SCANCODE_LSHIFT] || keys[SDL_SCANCODE_RSHIFT])?20:keys[SDL_SCANCODE_LCTRL]?1:5;
    float a=keys[SDL_SCANCODE_W]-keys[SDL_SCANCODE_S],b=keys[SDL_SCANCODE_D]-keys[SDL_SCANCODE_A];
    float lift=keys[SDL_SCANCODE_E]-keys[SDL_SCANCODE_Q],len=sqrtf(a*a+b*b+lift*lift);
    if(len>1)speed/=len;
    for(int k=0;k<3;k++){
        float v=speed*(-rows[6+k]*a+rows[k]*b+(k==1?lift:0));
        /* Exact integral of first-order velocity easing over this frame. */
        float old=g->cinema.velocity[k],move=v*step+(old-v)*ease/12;
        g->cinema.velocity[k]=old+(v-old)*ease;
        if(g->cinema.mode==2)g->cinema.mount[k]+=move;else g->cinema.eye[k]+=move;
    }
    if(g->cinema.mode==2){
        for(int k=0;k<3;k++){
            g->cinema.eye[k]=pos[k];for(int j=0;j<3;j++)g->cinema.eye[k]+=rot[k*3+j]*g->cinema.mount[j];
        }
        float world[9];for(int a=0;a<3;a++)for(int k=0;k<3;k++){
            world[a*3+k]=0;for(int j=0;j<3;j++)world[a*3+k]+=rows[a*3+j]*rot[k*3+j];
        }memcpy(rows,world,sizeof rows);
    }
    float target[3],f[3],length=0;
    for(int k=0;k<3;k++){target[k]=pos[k]+rot[k*3+1]*.65f;f[k]=target[k]-g->cinema.eye[k];length+=f[k]*f[k];}
    if(g->cinema.aim && length>.0001f){
        length=sqrtf(length);for(int k=0;k<3;k++)f[k]/=length;
        float horizontal=sqrtf(f[0]*f[0]+f[2]*f[2]);
        if(horizontal>.0001f){
            rows[0]=-f[2]/horizontal;rows[1]=0;rows[2]=f[0]/horizontal;
            rows[3]=-f[1]*rows[2];rows[4]=horizontal;rows[5]=f[1]*rows[0];
            for(int k=0;k<3;k++)rows[6+k]=-f[k];
            for(int k=0;k<3;k++){float r=rows[k],u=rows[3+k];rows[k]=r*cr+u*sr;rows[3+k]=u*cr-r*sr;}
            cinematic_angles(g,rows);
        }
    }
    if(g->cinema.focus_auto){
        g->cinema.focus=0;for(int k=0;k<3;k++)g->cinema.focus-=(target[k]-g->cinema.eye[k])*rows[6+k];
        if(g->cinema.focus<.2f)g->cinema.focus=10000; /* target behind a body-mounted lens */
    }
    memcpy(g->cinema.rows,rows,sizeof rows);memcpy(g->cinema.anchor,pos,sizeof pos);
    race_view_set_camera(g->view,g->cinema.eye,rows,g->cinema.projection[0],g->cinema.projection[1],.03f);
    race_view_show_car(g->view,1);g->start_cam_on=g->tv_on=0;
    race_view_set_grade(g->view,NULL);
}
static void cinematic_lens(const float pr[3],float aperture,float focus,const float sample[4],float shift[2],float jitter[2])
{
    gt4_photo_lens(12*pr[1],aperture,focus,4.f/3.f,pr[1],sample,shift,jitter);
    /* P: compensate the actual race viewport rather than the photo sensor.
       A point on the focus plane must project identically in every sample. */
    jitter[0]=shift[0]/focus*pr[0]*(448.f*4.f/3.f)/640*video_projection_x();
    jitter[1]=shift[1]/focus*pr[1];
}
