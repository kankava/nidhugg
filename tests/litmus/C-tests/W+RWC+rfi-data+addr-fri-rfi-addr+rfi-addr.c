/* Copyright (C) 2018 Magnus Lång and Tuan Phong Ngo
 * This benchmark is part of SWSC */

#include <assert.h>
#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

atomic_int vars[4]; 
atomic_int atom_0_r3_1; 
atomic_int atom_1_r1_1; 
atomic_int atom_1_r4_0; 
atomic_int atom_1_r7_1; 
atomic_int atom_1_r9_0; 
atomic_int atom_2_r3_1; 
atomic_int atom_2_r5_0; 

void *t0(void *arg){
label_1:;
  atomic_store_explicit(&vars[0], 1, memory_order_seq_cst);
  int v2_r3 = atomic_load_explicit(&vars[0], memory_order_seq_cst);
  int v3_r4 = v2_r3 ^ v2_r3;
  int v4_r4 = v3_r4 + 1;
  atomic_store_explicit(&vars[1], v4_r4, memory_order_seq_cst);
  int v36 = (v2_r3 == 1);
  atomic_store_explicit(&atom_0_r3_1, v36, memory_order_seq_cst);
  return NULL;
}

void *t1(void *arg){
label_2:;
  int v6_r1 = atomic_load_explicit(&vars[1], memory_order_seq_cst);
  int v7_r3 = v6_r1 ^ v6_r1;
  int v10_r4 = atomic_load_explicit(&vars[2+v7_r3], memory_order_seq_cst);
  atomic_store_explicit(&vars[2], 1, memory_order_seq_cst);
  int v12_r7 = atomic_load_explicit(&vars[2], memory_order_seq_cst);
  int v13_r8 = v12_r7 ^ v12_r7;
  int v16_r9 = atomic_load_explicit(&vars[3+v13_r8], memory_order_seq_cst);
  int v37 = (v6_r1 == 1);
  atomic_store_explicit(&atom_1_r1_1, v37, memory_order_seq_cst);
  int v38 = (v10_r4 == 0);
  atomic_store_explicit(&atom_1_r4_0, v38, memory_order_seq_cst);
  int v39 = (v12_r7 == 1);
  atomic_store_explicit(&atom_1_r7_1, v39, memory_order_seq_cst);
  int v40 = (v16_r9 == 0);
  atomic_store_explicit(&atom_1_r9_0, v40, memory_order_seq_cst);
  return NULL;
}

void *t2(void *arg){
label_3:;
  atomic_store_explicit(&vars[3], 1, memory_order_seq_cst);
  int v18_r3 = atomic_load_explicit(&vars[3], memory_order_seq_cst);
  int v19_r4 = v18_r3 ^ v18_r3;
  int v22_r5 = atomic_load_explicit(&vars[0+v19_r4], memory_order_seq_cst);
  int v41 = (v18_r3 == 1);
  atomic_store_explicit(&atom_2_r3_1, v41, memory_order_seq_cst);
  int v42 = (v22_r5 == 0);
  atomic_store_explicit(&atom_2_r5_0, v42, memory_order_seq_cst);
  return NULL;
}

int main(int argc, char *argv[]){
  pthread_t thr0; 
  pthread_t thr1; 
  pthread_t thr2; 

  atomic_init(&vars[3], 0);
  atomic_init(&vars[1], 0);
  atomic_init(&vars[0], 0);
  atomic_init(&vars[2], 0);
  atomic_init(&atom_0_r3_1, 0);
  atomic_init(&atom_1_r1_1, 0);
  atomic_init(&atom_1_r4_0, 0);
  atomic_init(&atom_1_r7_1, 0);
  atomic_init(&atom_1_r9_0, 0);
  atomic_init(&atom_2_r3_1, 0);
  atomic_init(&atom_2_r5_0, 0);

  pthread_create(&thr0, NULL, t0, NULL);
  pthread_create(&thr1, NULL, t1, NULL);
  pthread_create(&thr2, NULL, t2, NULL);

  pthread_join(thr0, NULL);
  pthread_join(thr1, NULL);
  pthread_join(thr2, NULL);

  int v23 = atomic_load_explicit(&atom_0_r3_1, memory_order_seq_cst);
  int v24 = atomic_load_explicit(&atom_1_r1_1, memory_order_seq_cst);
  int v25 = atomic_load_explicit(&atom_1_r4_0, memory_order_seq_cst);
  int v26 = atomic_load_explicit(&atom_1_r7_1, memory_order_seq_cst);
  int v27 = atomic_load_explicit(&atom_1_r9_0, memory_order_seq_cst);
  int v28 = atomic_load_explicit(&atom_2_r3_1, memory_order_seq_cst);
  int v29 = atomic_load_explicit(&atom_2_r5_0, memory_order_seq_cst);
  int v30_conj = v28 & v29;
  int v31_conj = v27 & v30_conj;
  int v32_conj = v26 & v31_conj;
  int v33_conj = v25 & v32_conj;
  int v34_conj = v24 & v33_conj;
  int v35_conj = v23 & v34_conj;
  if (v35_conj == 1) assert(0);
  return 0;
}
