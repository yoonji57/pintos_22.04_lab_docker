# -*- perl -*-
use tests::tests;
use tests::threads::alarm;
check_alarm (7 ); // 각 스레드가 7번씩 sleep -> 실제 스레드 sleep 동작은 alarm-wait.c에서 만들어짐 

// 스레드들이 깨어난 순서가 올바른가를 검사 
