sub check_alarm {
    my ($iterations) = @_; // 함수 인자 받음 
    our ($test); 

    @output = read_text_file ("$test.output"); // 테스트 결과 파일 읽음 
    common_checks ("run", @output); // 기본적인 공통 검사 

    my (@products); // 기대하는 wakeup 시각 목록을 담을 배열 
    for (my ($i) = 0; $i < $iterations; $i++) { // 각 스레드의 1번째 wakeup부터 $iterations번째 wakeup까지  
	for (my ($t) = 0; $t < 5; $t++) { // 스레드 5개 의미 
	    push (@products, ($i + 1) * ($t + 1) * 10); // 기대하는 wakeup 시각 계산 
	}
    }
    @products = sort {$a <=> $b} @products; // 오름차순 정렬 

    local ($_);
    foreach (@output) {
	fail $_ if /out of order/i;

	my ($p) = /product=(\d+)$/;
	next if !defined $p;

	my ($q) = shift (@products);
	fail "Too many wakeups.\n" if !defined $q;
	fail "Out of order wakeups ($p vs. $q).\n" if $p != $q; # FIXME
    }
    fail scalar (@products) . " fewer wakeups than expected.\n"
      if @products != 0;
    pass; // 여기까지 문제 없으면 테스트 통과 
}

1;
