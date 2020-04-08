# tcplog

This is basically a fork of some of the code behind the BSD logger command, with some of the extra unneeded stuff removed,
and other needed stuff added. Needed being a euphemism for "I want to integrate this with a C gcc destructor TCP rsyslog
to a remote host, where remote host is graylog/elasticsearch". This, combined with a memory slab allocator/leak prevention
mechanism I wrote (not presented here), allows for a program to log into graylog at various configured log levels at will,
and have PROGNAME (e.g. argv[1] strrchr'd back to the first forward slash) searchable from elasticsearch, but also to have
that program save in a ringbuffer all attempted log messages, so that abnormal termination will cause the destructor to 
retroactively log all messages, regardless of their log level, for debugging purposes.
