#include "RC_Controller_Header.h"
#include <stdio.h>

void app_main(void)
{
    #if RECEIVER_OR_TRANSMITTER 
    init_rc_controller();   
  
    #else 
    init_rc_controller();   
    unsigned char * data;

    while (1){
        data = get_data();
        printf("x1 = %d\t y1 = %d\t y2 = %d\t\n", *(data+0), *(data+1), *(data+2));
    }
    #endif
}
