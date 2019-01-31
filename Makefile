# Makefile for baoprog

CC=gcc
COPTS=-O2

default: baoprog baopatch

baoprog: baoprog.c
	$(CC) $(COPTS) baoprog.c -o baoprog

baopatch: baopatch.c
	$(CC) $(COPTS) baopatch.c -o baopatch
