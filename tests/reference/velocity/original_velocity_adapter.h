#pragma once
#include "../base_mapping/original_base_adapter.h"
#ifdef __cplusplus
extern "C" {
#endif
void velocity_ref_route(unsigned owner,unsigned originalIndex);
void velocity_ref_tick(void);
float velocity_ref_field(unsigned parameter);
unsigned velocity_ref_type(unsigned parameter);
void velocity_ref_node(unsigned owner,float* amount,float* held,unsigned* destination);
#ifdef __cplusplus
}
#endif
