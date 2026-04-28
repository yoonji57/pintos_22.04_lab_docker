/* Creates N threads, each of which sleeps a different, fixed
   duration, M times.  Records the wake-up order and verifies
   that it is valid. */
// N개의 스레드를 만들고, 각 스레드는 서로 다른 고정 sleep 시간을 M번 반복. 그 뒤 깨어난 순서를 기록하고 올바른지 검증함 
// 각 스레드는 서로 다른 주기로 깸. 깨어날 때마다 단순히 자기 id만 기록. 메인 함수가 나중에 그 id 기록을 바탕으로 iteration * duration 값을 복원. 그 값이 비내림차순이어야 timer_sleep()이 제때 스레드를 깨웠다고 판단 
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_single (void) 
{
  test_sleep (5, 1); // 5개의 스레드를 만들고, 각 스레드는 1번씩 sleep 
}

void
test_alarm_multiple (void) 
{
  test_sleep (5, 7); // 5개의 스레드를 만들고, 각 스레드는 7번씩 sleep 
}

/* Information about the test. */
struct sleep_test // 모든 sleeper 스레드가 공유하는 정보를 담음
  {
    int64_t start;              /* Current time at start of test. */
    int iterations;             /* Number of iterations per thread. */

    /* Output. */
    struct lock output_lock;    /* Lock protecting output buffer. */
    int *output_pos;            /* Current position in output buffer. */
  };

/* Information about an individual thread in the test. */
struct sleep_thread  // 개별 스레드 정보 
  {
    struct sleep_test *test;     /* Info shared between all threads. */
    int id;                     /* Sleeper ID. */
    int duration;               /* Number of ticks to sleep. */
    int iterations;             /* Iterations counted so far. */
  };

static void sleeper (void *);

/* Runs THREAD_CNT threads thread sleep ITERATIONS times each. */
static void
test_sleep (int thread_cnt, int iterations)  -> thread_cnt 개의 스레드를 만들어 iterations 번씩 sleep 시킴 
{
  struct sleep_test test;
  struct sleep_thread *threads;
  int *output, *op;
  int product;
  int i;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs); // mlfqs 켜져 있으면 테스트 중단 -> 이 테스트는 priority 전제 

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations);
  msg ("Thread 0 sleeps 10 ticks each time,"); 
  msg ("thread 1 sleeps 20 ticks each time, and so on."); 
  msg ("If successful, product of iteration count and");
  msg ("sleep duration will appear in nondescending order.");

  /* Allocate memory. */
  threads = malloc (sizeof *threads * thread_cnt);
  output = malloc (sizeof *output * iterations * thread_cnt * 2);
  if (threads == NULL || output == NULL)
    PANIC ("couldn't allocate memory for test");

  /* Initialize test. */
  test.start = timer_ticks () + 100; // 현재 tick 보다 100 tick 뒤를 공통 시작 시점으로 잡음 -> 스레드들 생성되고 준비될 시간 약간 주는 것 
  test.iterations = iterations;
  lock_init (&test.output_lock);
  test.output_pos = output;

  /* Start threads. */
  ASSERT (output != NULL);
  for (i = 0; i < thread_cnt; i++) // thread_cnt 개의 스레드를 만듦 
    {
      struct sleep_thread *t = threads + i;
      char name[16];
      
      t->test = &test; // 공유 테스트 구조체 주소 넣음 
      t->id = i;
      t->duration = (i + 1) * 10;
      t->iterations = 0; // 각 스레드 반복 횟수 0으로 초기화 

      snprintf (name, sizeof name, "thread %d", i);
      // 새 스레드를 만들고, 그 스레드를 sleeper(t) 를 실행하라는 의미 
      thread_create (name, PRI_DEFAULT, sleeper, t);
    }
  
  /* Wait long enough for all the threads to finish. */
  // 모든 sleeper 스레드가 다 깰 때까지 메인 테스트 스레드가 넉넉하게 기다리는 시간
  timer_sleep (100 + thread_cnt * iterations * 10 + 100);

  /* Acquire the output lock in case some rogue thread is still
     running. */
  lock_acquire (&test.output_lock);

  /* Print completion order. */
  product = 0; // 누적 곱셈 결과 저장 
  for (op = output; op < test.output_pos; op++)
    {
      struct sleep_thread *t; // 현재 스레드 정보 
      int new_prod; // 새로운 곱셈 결과 저장 
      ASSERT (*op >= 0 && *op < thread_cnt); // 유효한 스레드 ID 검증 
      t = threads + *op; // 그 ID에 해당하는 현재 스레드 정보 가져옴 

      new_prod = ++t->iterations * t->duration; // 이번 wakeup 의 product 계산 -> 검사 스크립트의 핵심 값과 같은 개념 
        
      msg ("thread %d: duration=%d, iteration=%d, product=%d", 
           t->id, t->duration, t->iterations, new_prod);
      
      // 테스트 실행 중 Pintos 안에서 직접 검사 -> wakeup 순서가 뒤집혔는지 
      // cf) alarm.pm은 Pintos 실행이 끝난 뒤 바깥에서 출력 파일을 읽어 최종 채점 
      if (new_prod >= product) // 새 product 가 이전 값 이상이면 정상. 이전 기준값 갱신 
        product = new_prod;
      else
        fail ("thread %d woke up out of order (%d > %d)!",
              t->id, product, new_prod);
    }

  /* Verify that we had the proper number of wakeups. */
  for (i = 0; i < thread_cnt; i++) // 각 스레드가 정확히 iterations 번씩 wakeup 했는지 검증 
    if (threads[i].iterations != iterations)
      fail ("thread %d woke up %d times instead of %d",
            i, threads[i].iterations, iterations);
  
  lock_release (&test.output_lock);
  free (output);
  free (threads);
}

/* Sleeper thread. */
// 깨어날 때마다 자신의 id만 기록
// 메인 함수가 나중에 그 id 기록을 바탕으로 iteration * duration 값을 복원. 그 값이 비내림차순이어야 timer_sleep()이 제때 스레드를 깨웠다고 판단 

static void
sleeper (void *t_) 
{
  struct sleep_thread *t = t_;
  struct sleep_test *test = t->test;
  int i;

  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * t->duration; // 이번 반복에서 깨어나야 할 절대 시각을 계산 
      timer_sleep (sleep_until - timer_ticks ()); // 현재 시각과 목표 시각 차이만큼 잠듦 
      lock_acquire (&test->output_lock); // 깨어난 뒤 출력 락을 잡음 
      *test->output_pos++ = t->id; // 자신의 id를 출력 배열에 기록하고 포인터를 다음 칸으로 옮김 
      lock_release (&test->output_lock); // 출력 락 해제 
    }
}

