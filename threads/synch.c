/* 이 파일은 교육용 운영체제 Nachos의 소스 코드를 바탕으로 작성되었다.
   Nachos 저작권 고지는 아래에 원문 그대로 재수록한다. */

/* 저작권 (c) 1992-1996 캘리포니아 대학교 이사회.
   모든 권리 보유.

   본 소프트웨어와 그 문서를 어떠한 목적이든 사용, 복사, 수정, 배포할 수 있는
   권한을, 사용료 없이 그리고 서면 계약 없이 부여한다. 단, 위 저작권 고지와
   아래 두 문단이 본 소프트웨어의 모든 사본에 포함되어야 한다.

   어떠한 경우에도 캘리포니아 대학교는 본 소프트웨어 및 문서의 사용으로 인해
   발생하는 직접적, 간접적, 특별, 부수적, 결과적 손해에 대해 책임지지 않는다.
   이는 캘리포니아 대학교가 그러한 손해의 가능성을 통지받은 경우에도 동일하다.

   캘리포니아 대학교는 상품성 및 특정 목적 적합성에 대한 묵시적 보증을 포함하되
   이에 한정되지 않는 일체의 보증을 명시적으로 부인한다. 본 소프트웨어는
   "있는 그대로(AS IS)" 제공되며, 캘리포니아 대학교는 유지보수, 지원, 업데이트,
   개선 또는 수정 사항을 제공할 의무가 없다.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* 세마포어 SEMA를 VALUE로 초기화한다. 세마포어는 0 이상의 정수 값과,
   그 값을 조작하는 두 가지 원자적 연산으로 이루어진다.

   - down 또는 "P": 값이 양수가 될 때까지 기다린 뒤 값을 감소시킨다.

   - up 또는 "V": 값을 증가시키며(대기 중인 스레드가 있으면 하나를 깨운다). */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}

/* 세마포어에 대한 down("P") 연산.
   SEMA 값이 양수가 될 때까지 기다린 다음 원자적으로 1 감소시킨다.

   이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 된다.
   인터럽트가 비활성화된 상태에서도 호출할 수 있지만, sleep이 발생하면
   다음에 스케줄된 스레드가 인터럽트를 다시 활성화할 수 있다.
   sema_down 함수이다. */
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		list_push_back (&sema->waiters, &thread_current ()->elem);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* 세마포어에 대한 down("P") 연산을 수행하되,
   세마포어 값이 0이 아닐 때만 수행한다.
   감소에 성공하면 true, 아니면 false를 반환한다.

   이 함수는 인터럽트 핸들러에서 호출할 수 있다. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* 세마포어에 대한 up("V") 연산.
   SEMA 값을 증가시키고, SEMA를 기다리는 스레드가 있으면 하나를 깨운다.

   이 함수는 인터럽트 핸들러에서 호출할 수 있다. */
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (!list_empty (&sema->waiters))
		thread_unblock (list_entry (list_pop_front (&sema->waiters),
					struct thread, elem));
	sema->value++;
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* 세마포어 자체 테스트.
   두 개의 스레드 사이에서 제어가 "핑퐁"처럼 오가도록 만든다.
   동작을 보고 싶다면 printf() 호출을 추가하라. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* sema_self_test()에서 사용하는 스레드 함수. */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* LOCK을 초기화한다. 락은 어떤 시점에도 최대 한 스레드만 보유할 수 있다.
   이 락은 "재귀적(recursive)"이지 않다. 즉, 현재 락을 가진 스레드가
   같은 락을 다시 획득하려 하면 오류다.

   락은 초기값이 1인 세마포어의 특수한 형태다.
   락과 이런 세마포어의 차이는 두 가지다.
   첫째, 세마포어는 값이 1보다 클 수 있지만, 락은 한 번에 한 스레드만
   소유할 수 있다. 둘째, 세마포어는 소유자 개념이 없어 한 스레드가
   down하고 다른 스레드가 up해도 되지만, 락은 같은 스레드가 획득과 해제를
   모두 수행해야 한다. 이러한 제약이 부담스럽다면 락 대신 세마포어를
   사용해야 한다는 신호다. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* LOCK을 획득한다. 필요하면 락을 사용할 수 있을 때까지 sleep한다.
   현재 스레드가 이미 해당 락을 보유한 상태여서는 안 된다.

   이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 된다.
   인터럽트 비활성화 상태에서도 호출할 수 있지만, sleep이 필요하면
   인터럽트가 다시 활성화된다. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	sema_down (&lock->semaphore);
	lock->holder = thread_current ();
}

/* LOCK 획득을 시도하고 성공하면 true, 실패하면 false를 반환한다.
   현재 스레드가 이미 해당 락을 보유한 상태여서는 안 된다.

   이 함수는 sleep하지 않으므로 인터럽트 핸들러 안에서 호출할 수 있다. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* LOCK을 해제한다. LOCK은 현재 스레드가 보유하고 있어야 한다.
   lock_release 함수이다.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트 핸들러 안에서
   락 해제를 시도하는 것은 의미가 없다. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* 현재 스레드가 LOCK을 보유하면 true, 아니면 false를 반환한다.
   (다른 스레드가 락을 보유하는지 검사하는 것은 경쟁 상태를 유발한다.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* 리스트 안의 세마포어 하나를 나타내는 요소. */
struct semaphore_elem {
	struct list_elem elem;              /* 리스트 요소. */
	struct semaphore semaphore;         /* 이 세마포어. */
};

/* 조건 변수 COND를 초기화한다.
   조건 변수는 한 코드 조각이 조건을 신호하고, 협력하는 다른 코드가
   그 신호를 받아 동작하도록 해준다. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* LOCK을 원자적으로 해제하고, 다른 코드가 COND에 신호를 보낼 때까지 기다린다.
   COND 신호를 받으면 반환 전에 LOCK을 다시 획득한다.
   이 함수를 호출하기 전에 LOCK을 보유하고 있어야 한다.

   이 함수가 구현한 모니터는 "Hoare" 방식이 아닌 "Mesa" 방식이다.
   즉, 신호 전송과 수신은 원자적 연산이 아니다.
   따라서 일반적으로 호출자는 대기가 끝난 뒤 조건을 다시 검사하고,
   필요하면 다시 기다려야 한다.

   하나의 조건 변수는 하나의 락과만 연결되지만,
   하나의 락은 여러 조건 변수와 연결될 수 있다.
   즉, 락에서 조건 변수로는 일대다 관계다.

   이 함수는 sleep할 수 있으므로 인터럽트 핸들러 안에서 호출하면 안 된다.
   인터럽트 비활성화 상태에서도 호출할 수 있지만, sleep이 필요하면
   인터럽트가 다시 활성화된다. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* COND(LOCK으로 보호됨)를 기다리는 스레드가 있다면,
   이 함수는 그중 하나에 신호를 보내 대기에서 깨운다.
   이 함수를 호출하기 전에 LOCK을 보유하고 있어야 한다.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트 핸들러 안에서
   조건 변수에 신호를 보내려는 시도는 의미가 없다. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* COND(LOCK으로 보호됨)를 기다리는 모든 스레드가 있다면 전부 깨운다.
   이 함수를 호출하기 전에 LOCK을 보유하고 있어야 한다.

   인터럽트 핸들러는 락을 획득할 수 없으므로, 인터럽트 핸들러 안에서
   조건 변수에 신호를 보내려는 시도는 의미가 없다. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
