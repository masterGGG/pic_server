vcl 4.0;
import std;
backend fdfs_01 
{
    .host = "10.1.1.197";
    .port = "12580";
}

#purge ip acl#
acl purge 
{
    "localhost";
    "127.0.0.1";
}

#choose backend according to url#
sub chooseBackend
{
    if (req.url ~ "^\/download.fcgi\?") {
        set req.backend_hint = fdfs_01;
    } else {
        return (synth(404,"No Find"));
    }
}


sub vcl_recv 
{
    if (req.method == "PURGE") {
        if (!client.ip ~ purge) {
            return (synth(405, "Not allowed."));
        }
        return (hash);
    }

       if (req.url ~ "^\/crossdomain.xml") {
            return (synth(200,"OK"));
        }        

        if (req.method != "GET" && req.method != "HEAD") {
            return (pipe);
        }
        return (hash);
}

sub vcl_hash 
{
    hash_data(req.url);
    if (req.http.host) {
        hash_data(req.http.host);
    } else {
        hash_data(server.ip);
    }
    return (lookup);
}
sub vcl_hit 
{
    if (req.method == "PURGE") {
#        set obj.ttl = 0s;
        return (synth(200, "Purged."));
    }
#    set obj.ttl = 7d;
    return(deliver);
}

sub vcl_miss 
{
    if (req.method == "PURGE") {
        return (synth(404, "Not in cache."));
    }
    call chooseBackend;
    return(fetch);
}

sub vcl_backend_response
{
    set beresp.grace = 30m;
    if (bereq.url ~ "\.(jpg|jpeg|gif|png)$") {
        set beresp.ttl = 7200s;
    }
    return (deliver);
}


sub vcl_deliver 
{
    if (obj.hits > 0){         # 判断如果命中了就在http响应首部设置X-Cache为HIT
       set resp.http.X-Cache = "HIT from " + server.ip;  
    } else {                   # 否则就在http响应首部设置X-Cache为MISS
       set resp.http.X-Cache = "MISS";
    }
    return (deliver);
}


sub vcl_pipe 
{
    return (pipe);
} 
