/*
 * Copyright (c) 1997, 1998 Kungliga Tekniska H�gskolan
 * (Royal Institute of Technology, Stockholm, Sweden). 
 * All rights reserved. 
 *
 * Redistribution and use in source and binary forms, with or without 
 * modification, are permitted provided that the following conditions 
 * are met: 
 *
 * 1. Redistributions of source code must retain the above copyright 
 *    notice, this list of conditions and the following disclaimer. 
 *
 * 2. Redistributions in binary form must reproduce the above copyright 
 *    notice, this list of conditions and the following disclaimer in the 
 *    documentation and/or other materials provided with the distribution. 
 *
 * 3. All advertising materials mentioning features or use of this software 
 *    must display the following acknowledgement: 
 *      This product includes software developed by Kungliga Tekniska 
 *      H�gskolan and its contributors. 
 *
 * 4. Neither the name of the Institute nor the names of its contributors 
 *    may be used to endorse or promote products derived from this software 
 *    without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND 
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE 
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE 
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE 
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL 
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS 
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) 
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY 
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE. 
 */

#include "krb5_locl.h"
#include <resolve.h>

RCSID("$Id$");

static int
add_string(char ***res, int *count, const char *string)
{
    char **tmp = realloc(*res, (*count + 1) * sizeof(**res));
    if(tmp == NULL)
	return ENOMEM;
    *res = tmp;
    if(string) {
	tmp[*count] = strdup(string);
	if(tmp[*count] == NULL)
	    return ENOMEM;
    } else
	tmp[*count] = NULL;
    (*count)++;
    return 0;
}

static krb5_error_code
srv_find_realm(krb5_context context, char ***res, int *count, 
	       const char *realm, const char *proto, const char *service)
{
    char domain[1024];
    char alt_domain[1024];
    krb5_error_code ret;
    struct dns_reply *r;
    struct resource_record *rr;

    snprintf(domain, sizeof(domain), "_%s._%s.%s.", service, proto, realm);
    
    r = dns_lookup(domain, "srv");
    if(r == NULL && context->srv_try_rfc2052) {
	snprintf(alt_domain, sizeof(alt_domain), "%s.%s.%s.", 
		 service, proto, realm);
	r = dns_lookup(alt_domain, "srv");
    }
    if(r == NULL && context->srv_try_txt)
	r = dns_lookup(domain, "txt");
    if(r == NULL && context->srv_try_rfc2052 && context->srv_try_txt)
	r = dns_lookup(alt_domain, "txt");
    if(r == NULL)
	return 0;

    for(rr = r->head; rr; rr = rr->next){
	if(rr->type == T_SRV){
	    char buf[1024];
	    *res = realloc(*res, (*count + 1) * sizeof(**res));
	    snprintf (buf, sizeof(buf),
		      "%s/%s:%u",
		      proto,
		      rr->u.srv->target,
		      rr->u.srv->port);
	    ret = add_string(res, count, buf);
	    if(ret)
		return ret;
	}else if(rr->type == T_TXT) {
	    ret = add_string(res, count, rr->u.txt);
	    if(ret)
		return ret;
	}
    }
    dns_free_data(r);
    return 0;
}

static krb5_error_code
get_krbhst (krb5_context context,
	    const krb5_realm *realm,
	    const char *conf_string,
	    char ***hostlist)
{
    char **res, **r;
    int count;
    krb5_error_code ret;

    res = krb5_config_get_strings(context, NULL, 
				  "realms", *realm, conf_string, NULL);
    for(r = res, count = 0; r && *r; r++, count++);

    if(context->srv_lookup) {
	char *s[] = { "udp", "tcp", "http" }, **q;
	for(q = s; q < s + sizeof(s) / sizeof(s[0]); q++) {
	    ret = srv_find_realm(context, &res, &count, *realm, *q, "kerberos");
	    if(ret) {
		krb5_config_free_strings(res);
		return ret;
	    }
	}
    }

    if(count == 0) {
	char buf[1024];
	snprintf(buf, sizeof(buf), "kerberos.%s", *realm);
	ret = add_string(&res, &count, buf);
	if(ret) {
	    krb5_config_free_strings(res);
	    return ret;
	}
    }
    add_string(&res, &count, NULL);
    *hostlist = res;
    return 0;
}

krb5_error_code
krb5_get_krb_admin_hst (krb5_context context,
			const krb5_realm *realm,
			char ***hostlist)
{
    return get_krbhst (context, realm, "admin_server", hostlist);
}

krb5_error_code
krb5_get_krbhst (krb5_context context,
		 const krb5_realm *realm,
		 char ***hostlist)
{
    return get_krbhst (context, realm, "kdc", hostlist);
}

krb5_error_code
krb5_free_krbhst (krb5_context context,
		  char **hostlist)
{
    char **p;

    for (p = hostlist; *p; ++p)
	free (*p);
    free (hostlist);
    return 0;
}
