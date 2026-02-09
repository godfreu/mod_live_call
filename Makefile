LOCAL_CFLAGS=-I./include -fPIC -Wall -Werror
LOCAL_LDFLAGS=-lwebsockets -lpthread

SOURCES=src/mod_live_call.c src/ws_client.c src/audio_buffer.c src/connection_pool.c src/jitter_buffer.c
OBJECTS=$(SOURCES:.c=.o)
TARGET=mod_live_call.so

# FreeSWITCH paths - adjust as needed
FS_INCLUDES=/usr/local/freeswitch/include
FS_MODULES=/usr/local/freeswitch/mod

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) -shared -Xlinker -x -o $@ $(OBJECTS) $(LOCAL_LDFLAGS)

%.o: %.c
	$(CC) $(LOCAL_CFLAGS) -I$(FS_INCLUDES) -c $< -o $@

clean:
	rm -f $(OBJECTS) $(TARGET)

install: $(TARGET)
	install -d $(FS_MODULES)
	install $(TARGET) $(FS_MODULES)
	install -d /usr/local/freeswitch/conf/autoload_configs
	install configs/live_call.conf.xml /usr/local/freeswitch/conf/autoload_configs

uninstall:
	rm -f $(FS_MODULES)/$(TARGET)
	rm -f /usr/local/freeswitch/conf/autoload_configs/live_call.conf.xml

.PHONY: all clean install uninstall
