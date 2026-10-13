/* Cached VM capture must retain both newer native playlist blobs.
   Native persistence P, exact byte retention C; negative changes a byte. */
#include "save.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"playlist cache %d: %s\n",__LINE__,#x);return 1;}}while(0)
int main(int argc,char **argv){
 gt4_save_init(NULL);ah_vm *vm=ah_vm_new();ah_obj *game=ah_module_new(vm,"game",NULL,NULL,NULL);
 unsigned char old[0x810]={0},fresh[2][0x810],out[0x810];
 const char *names[]={"bgm_playlist_ps2","slide_playlist_ps2"};
 for(int k=0;k<2;k++){CHECK(gt4_save_option_blob_set(names[k],old,sizeof old));for(int i=0;i<0x810;i++)fresh[k][i]=(unsigned char)(i*(k+1)+17);}
 gt4_save_restore(vm,game);
 for(int k=0;k<2;k++)CHECK(gt4_save_option_blob_set(names[k],fresh[k],sizeof fresh[k]));
 CHECK(gt4_save_capture(vm,game,0));
 if(argc>1)fresh[1][99]^=1;
 for(int k=0;k<2;k++){CHECK(gt4_save_option_blob_get(names[k],out,sizeof out));CHECK(!memcmp(out,fresh[k],sizeof out));}
 CHECK(gt4_save_capture(vm,game,0));
 for(int k=0;k<2;k++){CHECK(gt4_save_option_blob_get(names[k],out,sizeof out));CHECK(!memcmp(out,fresh[k],sizeof out));}
 ah_vm_free(vm);gt4_save_shutdown();puts("PASS both newer playlists retained across repeated stale menu captures");return 0;
}
