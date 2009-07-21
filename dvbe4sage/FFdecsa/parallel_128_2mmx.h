/* FFdecsa -- fast decsa algorithm
 *
 * Copyright (C) 2003-2004  fatih89r
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <mmintrin.h>

#ifdef _WINDOWS
#define MEMALIGN __declspec(align(16))
#else
#define MEMALIGN __attribute__((aligned(16)))
#endif

struct group_t{
  __m64 s1,s2;
};
typedef struct group_t group;

#define GROUP_PARALLELISM 128

group static inline FF0(){
  group res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x0ULL;
  res.s2.m64_u64=0x0ULL;
#else
  res.s1=(__m64)0x0ULL;
  res.s2=(__m64)0x0ULL;
#endif
  return res;
}

group static inline FF1(){
  group res;
#ifdef _WINDOWS
  res.s1.m64_u64=0xffffffffffffffffULL;
  res.s2.m64_u64=0xffffffffffffffffULL;
#else
  res.s1=(__m64)0xffffffffffffffffULL;
  res.s2=(__m64)0xffffffffffffffffULL;
#endif
  return res;
}

group static inline FFAND(const group& a,const group& b){
  group res;
  res.s1=_m_pand(a.s1,b.s1);
  res.s2=_m_pand(a.s2,b.s2);
  return res;
}

group static inline FFOR(const group& a,const group& b){
  group res;
  res.s1=_m_por(a.s1,b.s1);
  res.s2=_m_por(a.s2,b.s2);
  return res;
}

group static inline FFXOR(const group& a,const group& b){
  group res;
  res.s1=_m_pxor(a.s1,b.s1);
  res.s2=_m_pxor(a.s2,b.s2);
  return res;
}

group static inline FFNOT(const group& a){
  group res;
  res.s1=_m_pxor(a.s1,FF1().s1);
  res.s2=_m_pxor(a.s2,FF1().s2);
  return res;
}


/* 64 rows of 128 bits */

void static inline FFTABLEIN(unsigned char *tab, int g, unsigned char *data){
  *(((int *)tab)+2*g)=*((int *)data);
  *(((int *)tab)+2*g+1)=*(((int *)data)+1);
}

void static inline FFTABLEOUT(unsigned char *data, unsigned char *tab, int g){
  *((int *)data)=*(((int *)tab)+2*g);
  *(((int *)data)+1)=*(((int *)tab)+2*g+1);
}

void static inline FFTABLEOUTXORNBY(int n, unsigned char *data, unsigned char *tab, int g){
  int j;
  for(j=0;j<n;j++){
    *(data+j)^=*(tab+8*g+j);
  }
}


struct batch_t{
  __m64 s1,s2;
};
typedef struct batch_t batch;

#define BYTES_PER_BATCH 16

batch static inline B_FFAND(const batch& a,const batch& b){
  batch res;
  res.s1=_m_pand(a.s1,b.s1);
  res.s2=_m_pand(a.s2,b.s2);
  return res;
}

batch static inline B_FFOR(const batch& a,const batch& b){
  batch res;
  res.s1=_m_por(a.s1,b.s1);
  res.s2=_m_por(a.s2,b.s2);
  return res;
}

batch static inline B_FFXOR(const batch& a,const batch& b){
  batch res;
  res.s1=_m_pxor(a.s1,b.s1);
  res.s2=_m_pxor(a.s2,b.s2);
  return res;
}

batch static inline B_FFN_ALL_29(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x2929292929292929ULL;
  res.s2.m64_u64=0x2929292929292929ULL;
#else
  res.s1=(__m64)0x2929292929292929ULL;
  res.s2=(__m64)0x2929292929292929ULL;
#endif
  return res;
}
batch static inline B_FFN_ALL_02(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x0202020202020202ULL;
  res.s2.m64_u64=0x0202020202020202ULL;
#else
  res.s1=(__m64)0x0202020202020202ULL;
  res.s2=(__m64)0x0202020202020202ULL;
#endif
  return res;
}
batch static inline B_FFN_ALL_04(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x0404040404040404ULL;
  res.s2.m64_u64=0x0404040404040404ULL;
#else
  res.s1=(__m64)0x0404040404040404ULL;
  res.s2=(__m64)0x0404040404040404ULL;
#endif
  return res;
}
batch static inline B_FFN_ALL_10(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x1010101010101010ULL;
  res.s2.m64_u64=0x1010101010101010ULL;
#else
  res.s1=(__m64)0x1010101010101010ULL;
  res.s2=(__m64)0x1010101010101010ULL;
#endif
  return res;
}
batch static inline B_FFN_ALL_40(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x4040404040404040ULL;
  res.s2.m64_u64=0x4040404040404040ULL;
#else
  res.s1=(__m64)0x4040404040404040ULL;
  res.s2=(__m64)0x4040404040404040ULL;
#endif
  return res;
}
batch static inline B_FFN_ALL_80(){
  batch res;
#ifdef _WINDOWS
  res.s1.m64_u64=0x8080808080808080ULL;
  res.s2.m64_u64=0x8080808080808080ULL;
#else
  res.s1=(__m64)0x8080808080808080ULL;
  res.s2=(__m64)0x8080808080808080ULL;
#endif
  return res;
}

batch static inline B_FFSH8L(const batch& a,int n){
  batch res;
  res.s1=_m_psllqi(a.s1,n);
  res.s2=_m_psllqi(a.s2,n);
  return res;
}

batch static inline B_FFSH8R(const batch& a,int n){
  batch res;
  res.s1=_m_psrlqi(a.s1,n);
  res.s2=_m_psrlqi(a.s2,n);
  return res;
}

void static inline M_EMPTY(void){
  _m_empty();
}


#undef XOR_8_BY
#define XOR_8_BY(d,s1,s2)    do{ __m64 *pd=(__m64 *)(d), *ps1=(__m64 *)(s1), *ps2=(__m64 *)(s2); \
                                 *pd = _m_pxor( *ps1 , *ps2 ); }while(0)

#undef XOREQ_8_BY
#define XOREQ_8_BY(d,s)      do{ __m64 *pd=(__m64 *)(d), *ps=(__m64 *)(s); \
                                 *pd = _m_pxor( *ps, *pd ); }while(0)

#undef COPY_8_BY
#define COPY_8_BY(d,s)       do{ __m64 *pd=(__m64 *)(d), *ps=(__m64 *)(s); \
                                 *pd =  *ps; }while(0)

#undef BEST_SPAN
#define BEST_SPAN            8

#undef XOR_BEST_BY
#define XOR_BEST_BY(d,s1,s2) do{ XOR_8_BY(d,s1,s2); }while(0);

#undef XOREQ_BEST_BY
#define XOREQ_BEST_BY(d,s)   do{ XOREQ_8_BY(d,s); }while(0);

#undef COPY_BEST_BY
#define COPY_BEST_BY(d,s)    do{ COPY_8_BY(d,s); }while(0);
