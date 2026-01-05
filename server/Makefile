
SRCS := aesdsocket.c
# OBJS=$(SRCS:.c=.o)

# Implicits
CC = $(CROSS_COMPILE)gcc
CFLAGS = -g -Werror

all: default

default: aesdsocket

aesdsocket: $(SRCS)

clean:
	rm -f aesdsocket