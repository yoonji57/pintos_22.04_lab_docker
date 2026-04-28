/* Creates N threads, each of which sleeps a different, fixed
   duration, M times.  Records the wake-up order and verifies
   that it is valid. */

// 여러 스레드가 같은 시각에 자도록 했을 때 정말 같은 tick에 함께 깨어나는가 
// 10 0 0 패턴이 5번 반복되도록 함 

// 같은 wakeup tick을 가진 스레드들을 한 번에 깨우는지 확인 
// 어떤 스레드 하나만 먼저 깨우고 나머지를 다음 tick으로 미루지 않는지 확인 -> 하나만 깨우고... 다른애들은 못보는 경우 막기? 
// 같은 tick에 깨어난 스레드들이 모두 그 tick 안에서 실행될 수 있는지 확인 

// 같은 목표 시각이면 모두 같은 tick에 ready / running 상태로 넘어오느냐 
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static void test_sleep (int thread_cnt, int iterations);

void
test_alarm_simultaneous (void) 
{
  test_sleep (3, 5); // 3개의 스레드가 5번 반복. 매번 10tick 간격으로 같은 목표 시각에 깨어나게 설정 
  // (0) 10 10 10 20 20 20 30 30 30 
  // output
  // 10 0 0 10 0 0 10 0 0 10 0 0 -> 나오는게 목표표
}

/* Information about the test. */
struct sleep_test // 모든 worker 스레드가 공유하는 정보 
  {
    int64_t start;              /* Current time at start of test. */
    int iterations;             /* Number of iterations per thread. */
    int *output_pos;            /* Current position in output buffer. */
    // wakeup 시간을 기록할 버퍼 위치 
  };

static void sleeper (void *);

/* Runs THREAD_CNT threads thread sleep ITERATIONS times each. */
static void
test_sleep (int thread_cnt, int iterations) 
{
  struct sleep_test test;
  int *output;
  int i;

  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  msg ("Creating %d threads to sleep %d times each.", thread_cnt, iterations);
  msg ("Each thread sleeps 10 ticks each time.");
  msg ("Within an iteration, all threads should wake up on the same tick.");

  /* Allocate memory. */
  output = malloc (sizeof *output * iterations * thread_cnt * 2);
  if (output == NULL)
    PANIC ("couldn't allocate memory for test");

  /* Initialize test. */
  test.start = timer_ticks () + 100;
  test.iterations = iterations;
  test.output_pos = output;

  /* Start threads. */
  ASSERT (output != NULL);

  for (i = 0; i < thread_cnt; i++)
    {
      char name[16];
      snprintf (name, sizeof name, "thread %d", i);
      thread_create (name, PRI_DEFAULT, sleeper, &test);
    }
  
  /* Wait long enough for all the threads to finish. */
  timer_sleep (100 + iterations * 10 + 100); // 메인 스레드는 충분히 오래 잠들어 worker들이 다 끝나길 기다림 
// 그럼 아래 코드는? 

  /* Print completion order. */
  msg ("iteration 0, thread 0: woke up after %d ticks", output[0]);
  for (i = 1; i < test.output_pos - output; i++) 
    msg ("iteration %d, thread %d: woke up %d ticks later",
         i / thread_cnt, i % thread_cnt, output[i] - output[i - 1]); 
  
  free (output);
}

/* Sleeper thread. */
static void
sleeper (void *test_) 
{
  struct sleep_test *test = test_;
  int i;

  /* Make sure we're at the beginning of a timer tick. */
  timer_sleep (1); // 각 스레드를 tick 경계 근처로 맞추려는 준비 단계? 왜 아까 코드 안써 

  // // timer_ticks()가 바뀔 때까지 아주 짧게 busy-wait -> tick 시작 직후에 맞춰서 sleep 계산을 하기 위함 
  // int64_t start_time = timer_ticks ();
  // while (timer_elapsed (start_time) == 0)
  //   continue;

  // 위 두개 코드 차이가 뭐임 


  for (i = 1; i <= test->iterations; i++) 
    {
      int64_t sleep_until = test->start + i * 10;
      timer_sleep (sleep_until - timer_ticks ());
      *test->output_pos++ = timer_ticks () - test->start; // 깨어난 시각을 기록 -> 배열에는 10, 10, 10, 20, 20, 20 ... 같은 값이 들어와야 함
      thread_yield (); // 같은 tick에 깨어난 다른 스레드도 바로 CPU를 받아 기록할 수 있게 양보하는 역할



      // 이게 중요한건가 ?yeild가? 이걸 잘 구현해야되는거임? 
      //  timer_sleep()... 그리고 timer_interrupt(), 간접적으로는 thread_block()랑 thread_unblock()랑 엮여있음

      // timer_sleep() 가 스레드를 올바르게 재우는가
      // time interrupt가 매 tick마다 자는 스레드들을 검사
      // 깨어날 시간이 된 스레드들을 빠짐없이 ready로 바꿈
      // 같은 시각이면 여러 개를 한 번에 깨움

      // 지금 이건 단일스레드 기준? 엉. 
    }
}
