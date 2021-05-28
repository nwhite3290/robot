final:
	gcc -pthread -o final final.c slowdown.s speedup.s pca9685/pca9685.c -lwiringPi
debug:
	gcc -pthread -g -o debug final.c slowdown.s speedup.s pca9685/pca9685.c -lwiringPi
clean:
	rm final
