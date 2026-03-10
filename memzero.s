.global memzero
.type memzero, @function
memzero:
	testq %rsi, %rsi
	jz done
	xorq %rax, %rax
	rep stosb
done:
	ret
