/* Copyright (C) 2018 Magnus Lång and Tuan Phong Ngo
 * This benchmark is part of SWSC */

#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

atomic_int vars[3]; 
atomic_int atom_0_r3_0; 
atomic_int atom_1_r6_0; 
atomic_int atom_1_r8_1; 

void *t0(void *arg){
label_1:;
  atomic_store_explicit(&vars[0], 2, memory_order_seq_cst);

  int v2_r3 = atomic_load_explicit(&vars[1], memory_order_seq_cst);
  int v19 = (v2_r3 == 0);
  atomic_store_explicit(&atom_0_r3_0, v19, memory_order_seq_cst);
  return NULL;
}

void *t1(void *arg){
label_2:;
  atomic_store_explicit(&vars[1], 1, memory_order_seq_cst);
  int v4_r3 = atomic_load_explicit(&vars[2], memory_order_seq_cst);
  int v5_r5 = v4_r3 ^ v4_r3;
  int v8_r6 = atomic_load_explicit(&vars[0+v5_r5], memory_order_seq_cst);
  int v10_r8 = atomic_load_explicit(&vars[0], memory_order_seq_cst);
  int v20 = (v8_r6 == 0);
  atomic_store_explicit(&atom_1_r6_0, v20, memory_order_seq_cst);
  int v21 = (v10_r8 == 1);
  atomic_store_explicit(&atom_1_r8_1, v21, memory_order_seq_cst);
  return NULL;
}

void *t2(void *arg){
label_3:;
  atomic_store_explicit(&vars[0], 1, memory_order_seq_cst);
  return NULL;
}

int main(int argc, char *argv[]){
  pthread_t thr0; 
  pthread_t thr1; 
  pthread_t thr2; 

  atomic_init(&vars[0], 0);
  atomic_init(&vars[1], 0);
  atomic_init(&vars[2], 0);
  atomic_init(&atom_0_r3_0, 0);
  atomic_init(&atom_1_r6_0, 0);
  atomic_init(&atom_1_r8_1, 0);

  pthread_create(&thr0, NULL, t0, NULL);
  pthread_create(&thr1, NULL, t1, NULL);
  pthread_create(&thr2, NULL, t2, NULL);

  pthread_join(thr0, NULL);
  pthread_join(thr1, NULL);
  pthread_join(thr2, NULL);

  int v11 = atomic_load_explicit(&atom_0_r3_0, memory_order_seq_cst);
  int v12 = atomic_load_explicit(&vars[0], memory_order_seq_cst);
  int v13 = (v12 == 2);
  int v14 = atomic_load_explicit(&atom_1_r6_0, memory_order_seq_cst);
  int v15 = atomic_load_explicit(&atom_1_r8_1, memory_order_seq_cst);
  int v16_conj = v14 & v15;
  int v17_conj = v13 & v16_conj;
  int v18_conj = v11 & v17_conj;
  if (v18_conj == 1) assert(0);
  return 0;
}
