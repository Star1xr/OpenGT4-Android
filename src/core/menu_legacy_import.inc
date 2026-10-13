/* Native local-file boundary for the US edition's authored inheritance.
   001D4500 +48..58 and 001D4598 +50..58 both select the GT3 US file.
   Therefore US "inheritGT4pData" imports GT3 B/A completion, not Prologue.
   P: extracted source file lives in profiles/imports, no console device. */
static int nat_legacy_import(ah_vm *vm,ah_obj *self,void *data,int argc,const ah_val *argv,ah_val *ret){
 (void)self;(void)argc;(void)argv;const char *op=data;menu *m=ah_user(vm);
 int license=strstr(op,"GT4p")!=NULL,apply=!strncmp(op,"inherit",7);ah_obj *game=game_object(m);
 const char *flag=license?"withdrawnGT4p":"withdrawn";ah_val done=ah_nil();ah_get_member(vm,game,flag,&done);
 if(apply && ah_truthy(done)){*ret=ah_str(vm,"OK");return 0;}
 const char *directory=gt4_save_directory();char path[1200];
 snprintf(path,sizeof path,"%s/imports/BASCUS-97102GAMEDATA/BASCUS-97102GAMEDATA",directory?directory:"");
 plat_file *file=directory && *directory?plat_file_open(path):NULL;
 if(!file){*ret=ah_str(vm,"FILE_NOT_FOUND");return 0;}
 uint64_t size=plat_file_size(file);uint8_t *bytes=size>=0x78 && size<=2*1024*1024?malloc((size_t)size):NULL;
 int valid=bytes && plat_file_read_at(file,0,bytes,(size_t)size)==size;plat_file_close(file);
 if(!valid){free(bytes);*ret=ah_str(vm,"DEVICE_ERROR");return 0;}
 if(!apply){free(bytes);*ret=ah_str(vm,"OK");return 0;}
 uint8_t *snapshot=NULL;size_t snapshot_size=0;
 if(gt4_save_active()<=0 || !gt4_state_pack(vm,game,&snapshot,&snapshot_size)){free(bytes);*ret=ah_str(vm,"DEVICE_ERROR");return 0;}
 if(license){
  /* 001D4698 +A0..114: complement the two indices, then the low byte's
     bit7 for eight exams per group, 0x154 stride. A requires B as well. */
  uint64_t first=(uint64_t)(~car_u32(bytes+0x70))*0x204u+(uint64_t)(~car_u32(bytes+0x74))*0x44u+0x170u;
  if(first+15u*0x154u>=size){free(snapshot);free(bytes);*ret=ah_str(vm,"DEVICE_ERROR");return 0;}
  int pass[2]={1,1};for(int group=0;group<2;group++)for(int i=0;i<8;i++)if(!(bytes[first+(group*8+i)*0x154]&0x80))pass[group]=0;
  if(!pass[0])pass[1]=0;
  if(!pass[0]){free(snapshot);free(bytes);*ret=ah_str(vm,"NO_DATA");return 0;}
  ah_val records;ah_get_member(vm,game,"race_record",&records);
  /* 001D4698 +1D0..260: l0b/l0a 0001..0016, update(type1, grade12),
     only when the existing result is not already passed. */
  for(int group=0;group<2;group++)if(pass[group])for(int i=1;i<=16;i++){
   char code[32];snprintf(code,sizeof code,"l0%c%04d",group?'a':'b',i);ah_val arg=ah_str(vm,code),unit;
   if(ah_call_member(vm,records.u.o,"getUnit",1,&arg,&unit)<0){free(snapshot);free(bytes);return -1;}
   ah_val word;ah_get_member(vm,unit.u.o,"original_record",&word);uint32_t bits=(uint32_t)ah_num(word);
   if(!gt4_career_record_meets_grade((uint8_t)bits,12)){gt4_career_record_update(&bits,1,12);ah_set_member(vm,unit.u.o,"original_record",ah_int(bits));}
  }
 }else{
  /* 001D49C8 +5C..D0: complement u64 +50, reject zero, cap at 100000.
     US uses credits directly; only JP divides by100. */
  uint64_t amount=~((uint64_t)car_u32(bytes+0x50)|((uint64_t)car_u32(bytes+0x54)<<32));
  if(!amount){free(snapshot);free(bytes);*ret=ah_str(vm,"NO_DATA");return 0;}
  if(amount>100000)amount=100000;ah_val arg=ah_int((int64_t)amount);
  if(ah_call_member(vm,game,"incMoney",1,&arg,ret)<0){free(snapshot);free(bytes);return -1;}
 }
 free(bytes);ah_set_member(vm,game,flag,ah_int(1));
 if(!gt4_save_capture(vm,game,1) || !gt4_save_store()){gt4_state_merge(vm,game,snapshot,snapshot_size);gt4_game_bind(vm,game);gt4_save_capture(vm,game,1);free(snapshot);*ret=ah_str(vm,"DEVICE_ERROR");return 0;}
 free(snapshot);
 *ret=ah_str(vm,"OK");return 0;
}
