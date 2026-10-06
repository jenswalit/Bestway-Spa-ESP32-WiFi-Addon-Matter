#pragma once
#include "esp_log.h"
#include <string>
#include <map>
#include <vector>
#include <cstring>
using nvs_handle_t=int;
constexpr int NVS_READONLY=0,NVS_READWRITE=1,ESP_ERR_NVS_NOT_FOUND=1;
inline std::map<std::string,std::vector<unsigned char>> committed,pending;
inline bool fail_commit=false;
inline int nvs_open(const char*,int,nvs_handle_t* h){*h=1;pending=committed;return ESP_OK;}
inline void nvs_close(nvs_handle_t){}
inline int nvs_set_blob(nvs_handle_t,const char*k,const void*p,size_t n){auto b=(const unsigned char*)p;pending[k]={b,b+n};return ESP_OK;}
inline int nvs_get_blob(nvs_handle_t,const char*k,void*p,size_t*n){if(!committed.count(k))return ESP_ERR_NVS_NOT_FOUND;auto&v=committed[k];if(*n<v.size())return -2;std::memcpy(p,v.data(),v.size());*n=v.size();return ESP_OK;}
inline int nvs_set_u8(nvs_handle_t h,const char*k,uint8_t v){return nvs_set_blob(h,k,&v,sizeof v);}
inline int nvs_set_u16(nvs_handle_t h,const char*k,uint16_t v){return nvs_set_blob(h,k,&v,sizeof v);}
inline int nvs_get_u8(nvs_handle_t h,const char*k,uint8_t*v){size_t n=sizeof *v;return nvs_get_blob(h,k,v,&n);}
inline int nvs_get_u16(nvs_handle_t h,const char*k,uint16_t*v){size_t n=sizeof *v;return nvs_get_blob(h,k,v,&n);}
inline int nvs_erase_key(nvs_handle_t,const char*k){pending.erase(k);return ESP_OK;}
inline int nvs_commit(nvs_handle_t){if(fail_commit)return ESP_FAIL;committed=pending;return ESP_OK;}
