@ decrease_speed.s	takes speed as input and decreases by 200
@					returns new speed
@
@ 5.16.2021
@ Cody McKinney


@ Define Pi
	.cpu	cortex-a53
	.fpu	neon-fp-armv8
	.syntax	unified

action:
	.asciz	"speed decrease"
update:
	.asciz	"speed is now %d"
warn:
	.asciz	"speed is at minimum"

@ Program Code
	.text
	.align	2
	.global	decrease_speed
	.type	decrease_speed, %function

decrease_speed:
	push	{fp, lr}
	add		fp, sp, #4
	sub		sp, sp, #8
	
	str		r0, [fp, #-8]		@ storing speed onto stack [fp, #-8] : speed
	
	ldr		r0, =action			@ print (speed decrease)
	bl		printf
	
	mov		r0, #10
	bl		putchar
	
	ldr		r3, [fp, #-8]		@ r3 = speed
	cmp		r3, #200			@ speed > 200
	ble		minimum
	
	sub		r3, r3, #200		@ speed -= 200
	str		r3, [fp, #-8]
	mov		r1, r3				@ r1 = speed
	
	ldr		r0, =update
	bl		printf
	
	mov		r0, #10
	bl		putchar
	b		end
	
minimum:
	ldr		r0, =warn
	bl		printf
	
end:
	ldr		r0, [fp, #-8]
	sub		sp, fp, #4
	pop		{fp, pc}
