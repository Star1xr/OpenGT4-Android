/* Disc/PC manager isolation, with real authored menu data and save API. */
#include "../../src/core/menu.c"
#define CHECK(x) do{if(!(x)){fprintf(stderr,"demo menu line %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv)
{
    CHECK(argc>=3);gt4_save_init(argv[2]);const char *why;char error[256];
    gt4_vol *vol=gt4_vol_open(argv[1],&why);CHECK(vol);
    menu *m=menu_open(vol,"labomode",error,sizeof error);CHECK(m);
    ah_vm *vm=m->vm;ah_val result;
    ah_obj *demo=ah_module_new(vm,"demo_test",NULL,NULL,NULL);
    ah_obj *sub=ah_module_new(vm,"sub_test",NULL,NULL,NULL);
    replay_manager(vm,demo,"MODE_DEMONSTRATION");
    replay_manager(vm,sub,"MODE_DEMONSTRATION_SUB");
    ah_val count;CHECK(ah_get_member(vm,demo,"count",&count) && ah_num(count)==13);
    for(int k=0;k<13;k++){
        ah_val index=ah_int(k),valid,texture;CHECK(ah_call_member(vm,demo,"getFile",1,&index,&result)>=0);
        ah_obj *item=ah_as_obj(result,AO_MODULE);CHECK(item);
        CHECK(ah_call_member(vm,item,"isIconValid",0,NULL,&valid)>=0 && ah_truthy(valid));
        CHECK(ah_call_member(vm,item,"getIconTexture",0,NULL,&texture)>=0 && ah_cstr(texture));
        CHECK(strstr(ah_cstr(texture),"disc/icon//replay/Rdemo"));
    }
    ah_val i=ah_int(0);CHECK((ah_call_member(vm,demo,"getFile",1,&i,&result)>=0));
    ah_obj *first=ah_as_obj(result,AO_MODULE);CHECK(first);
    if(argc>3)ah_set_member(vm,first,"_name",ah_str(vm,"/replay/missing/missing"));
    CHECK((ah_call_member(vm,first,"load",0,NULL,&result)>=0));CHECK(!strcmp(ah_cstr(result),"OK"));
    CHECK(!strcmp(replay_loaded,"/replay/Rdemo0/Rdemo0"));
    /* Changing another manager must not redirect an existing PC file. */
    gt4_replay_file file={0},loaded;uint8_t tape[32]={0};char name[64];
    strcpy(file.course,"tsukuba");strcpy(file.car,"ka_01");file.tape=tape;file.steps=1;
    CHECK(gt4_replay_store(&file,name,sizeof name));
    ah_obj *pc=replay_file_object(vm,0,name,&file,0,0);
    replay_manager_files(vm,demo,"MODE_DEMONSTRATION");
    CHECK((ah_call_member(vm,pc,"load",0,NULL,&result)>=0));CHECK(!strcmp(ah_cstr(result),"OK") && !strcmp(replay_loaded,name));
    replay_to_save=&file;ah_set_member(vm,pc,"title",ah_str(vm,"demo regression"));
    CHECK((ah_call_member(vm,pc,"saveNew",0,NULL,&result)>=0));CHECK(!strcmp(ah_cstr(result),"OK"));
    CHECK(gt4_replay_load(name,&loaded));CHECK(!strcmp(loaded.title,"demo regression"));gt4_replay_free(&loaded);
    replay_to_save=NULL;CHECK((ah_call_member(vm,pc,"remove",0,NULL,&result)>=0) && ah_truthy(result));
    char names[256][64];int n=gt4_replay_list(names,256);
    for(int k=0;k<n;k++)CHECK(strcmp(names[k],name));
    CHECK((ah_call_member(vm,first,"remove",0,NULL,&result)>=0) && !ah_truthy(result));
    puts("PASS separate Demo/Sub managers, original disc path, PC load/overwrite/delete retain file ownership; negative substitutes missing disc file.");
    menu_close(m);gt4_vol_close(vol);gt4_save_shutdown();return 0;
}
