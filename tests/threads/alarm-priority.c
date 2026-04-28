/* Checks that when the alarm clock wakes up threads, the
   higher-priority threads run first. */
// 여러 스레드가 같은 시각에 꺠어나면 우선순위가 높은 스레드가 먼저 실행되어야 함 
// 같은 시간에 unblock된 스레드들을 ready queue에 넣을 때 priority를 고려하는지를 확인하는 것 
#include <stdio.h>
#include "tests/threads/tests.h"
#include "threads/init.h"
#include "threads/malloc.h"
#include "threads/synch.h"
#include "threads/thread.h"
#include "devices/timer.h"

static thread_func alarm_priority_thread;
static int64_t wake_time;
static struct semaphore wait_sema;

void
test_alarm_priority (void) 
{
  int i;
  
  /* This test does not work with the MLFQS. */
  ASSERT (!thread_mlfqs);

  wake_time = timer_ticks () + 5 * TIMER_FREQ; // 공통 기상 시각 설정 
  sema_init (&wait_sema, 0); // 메인 스레드가 자식 10개가 모두 끝날 때까지 기다리기 위한 세마포어 
  
  for (i = 0; i < 10; i++) // 우선순위가 서로 다른 10개의 스레드 생성 
    {
      int priority = PRI_DEFAULT - (i + 5) % 10 - 1; // 그냥 대충 랜덤 느낌으로 생성되게 계산식 설정한거 -> 실제 생성 priority 순서는 25, 24, 23, 22, 21, 30, 29, 28, 27, 26
      char name[16];
      snprintf (name, sizeof name, "priority %d", priority);
      thread_create (name, priority, alarm_priority_thread, NULL);
    }

  thread_set_priority (PRI_MIN); // 메인 스레드 우선순위를 최저로 낮춰서? 깨어난 worker들이 먼저 돌게 만들어? -> 이때 바로 context switch가 발생하는 것은 아님. 아래 for문은 그대로 진행되면서 실행됨 

  for (i = 0; i < 10; i++) // 메인 스레드는 sema_down을 10번 호출하면서 자식 스레드 10개가 모두 끝날 때까지 기다림 
    sema_down (&wait_sema);
}

static void
alarm_priority_thread (void *aux UNUSED) 
{
  /* Busy-wait until the current time changes. */
  // timer_ticks()가 바뀔 때까지 아주 짧게 busy-wait -> tick 시작 직후에 맞춰서 sleep 계산을 하기 위함 
  int64_t start_time = timer_ticks ();
  while (timer_elapsed (start_time) == 0)
    continue;

  /* Now we know we're at the very beginning of a timer tick, so
     we can call timer_sleep() without worrying about races
     between checking the time and a timer interrupt. */
  timer_sleep (wake_time - timer_ticks ());

  /* Print a message on wake-up. */
  msg ("Thread %s woke up.", thread_name ());

// 큐에 언제 넣음? 
// thread_create()로 새 스레드 만들었을 때 마지막에 thread_unblock(t) 호출해서 ready queue에 넣음 
// thread_unblock()에서 ready_list에 넣음 
// 세마포어 up도 sema_up() 에서 기다리던 스레드를 thread_unblock() 해서 깨움 
// thread_yeild() 에서 running 스레드가 스스로 양보할 때 현재 스레드를 ready_list에 넣음 
// -> ready_list에 넣을 때때 priority 순으로 정렬해서 넣기 ? 

// 뽑는 부분은 threads/thread 안에 있는 next_thread_to_run() 함수에서 ready queue에 있는 스레드 중 priority가 가장 높은 스레드가 먼저 실행되도록 shceduler 구현해야됨 

// 넣을 떄 정렬이나 뽑을 때 선택 중에 하나 골라서 구현? 

  sema_up (&wait_sema); // 메인 스레드에 끝났음을 알림 

  // 여기서 세마를 왜씀? 
  // alarm 기능을 구현하려고 쓰는 게 아니라, 테스트 진행을 동기화하기 위함 -> 메인 테스트 스레드가 worker 스레드 10개가 모두 끝날 때까지 기다리게 하기 위함 
  // 부모 스레드가 자식 스레드가 종료되는 것을 기다리고자 할 때 sema 0으로 초기화해서 사용하기도 함 
}
