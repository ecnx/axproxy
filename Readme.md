Purpose
-------
A portable SOCKS-5 Server with IPv6 support

Building
--------
```
make
```

Example
-------
```
axproxy 0.0.0.0:8080 # Listen on port 8080
axproxy [::1]:8181   # Listen on port 8181
```

Help message
------------

```
[axpr] AxProxy - ver. 1.05.1a
[axpr] usage: axproxy [-vd] listen-addr:listen-port

       option -v         Enable verbose logging
       option -d         Run in background
       listen-addr       Listen address
       listen-port       Listen port

Note: Both IPv4 and IPv6 can be used

```
