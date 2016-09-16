CC ?= g++

CFLAGS =-Wall -O2 -Werror

OBJFILES = datapipe.o

EXECUTABLE = datapipe

$(EXECUTABLE): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(EXECUTABLE) $(OBJFILES)

clean: 		
			rm $(OBJFILES) $(EXECUTABLE)