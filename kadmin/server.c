/*
 * Copyright (c) 1997-1999 Kungliga Tekniska H�gskolan
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

#include "kadmin_locl.h"
#include <krb5-private.h>

RCSID("$Id$");

static kadm5_ret_t
kadmind_dispatch(void *kadm_handle, krb5_data *in, krb5_data *out)
{
    kadm5_ret_t ret;
    int32_t cmd, mask, tmp;
    kadm5_server_context *context = kadm_handle;
    char client[128], name[128], name2[128];
    char *op = "";
    krb5_principal princ, princ2;
    kadm5_principal_ent_rec ent;
    char *password, *exp;
    krb5_keyblock *new_keys;
    int n_keys;
    char **princs;
    int n_princs;
    krb5_storage *sp;
    
    krb5_unparse_name_fixed(context->context, context->caller, 
			    client, sizeof(client));
    
    sp = krb5_storage_from_data(in);

    krb5_ret_int32(sp, &cmd);
    switch(cmd){
    case kadm_get:{
	op = "GET";
	ret = krb5_ret_principal(sp, &princ);
	if(ret)
	    goto fail;
	ret = krb5_ret_int32(sp, &mask);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	krb5_unparse_name_fixed(context->context, princ, name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
	ret = _kadm5_acl_check_permission(context, KADM5_PRIV_GET);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	ret = kadm5_get_principal(kadm_handle, princ, &ent, mask);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	if(ret == 0){
	    kadm5_store_principal_ent(sp, &ent);
	    kadm5_free_principal_ent(kadm_handle, &ent);
	}
	krb5_free_principal(context->context, princ);
	break;
    }
    case kadm_delete:{
	op = "DELETE";
	ret = krb5_ret_principal(sp, &princ);
	if(ret)
	    goto fail;
	krb5_unparse_name_fixed(context->context, princ, name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
	ret = _kadm5_acl_check_permission(context, KADM5_PRIV_DELETE);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	ret = kadm5_delete_principal(kadm_handle, princ);
	krb5_free_principal(context->context, princ);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	break;
    }
    case kadm_create:{
	op = "CREATE";
	ret = kadm5_ret_principal_ent(sp, &ent);
	if(ret)
	    goto fail;
	ret = krb5_ret_int32(sp, &mask);
	if(ret){
	    kadm5_free_principal_ent(context->context, &ent);
	    goto fail;
	}
	ret = krb5_ret_string(sp, &password);
	if(ret){
	    kadm5_free_principal_ent(context->context, &ent);
	    goto fail;
	}
	krb5_unparse_name_fixed(context->context, ent.principal, 
				name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
	ret = _kadm5_acl_check_permission(context, KADM5_PRIV_ADD);
	if(ret){
	    kadm5_free_principal_ent(context->context, &ent);
	    memset(password, 0, strlen(password));
	    free(password);
	    goto fail;
	}
	ret = kadm5_create_principal(kadm_handle, &ent, 
				     mask, password);
	kadm5_free_principal_ent(kadm_handle, &ent);
	memset(password, 0, strlen(password));
	free(password);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	break;
    }
    case kadm_modify:{
	op = "MODIFY";
	ret = kadm5_ret_principal_ent(sp, &ent);
	if(ret)
	    goto fail;
	ret = krb5_ret_int32(sp, &mask);
	if(ret){
	    kadm5_free_principal_ent(context, &ent);
	    goto fail;
	}
	krb5_unparse_name_fixed(context->context, ent.principal, 
				name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
	ret = _kadm5_acl_check_permission(context, KADM5_PRIV_MODIFY);
	if(ret){
	    kadm5_free_principal_ent(context, &ent);
	    goto fail;
	}
	ret = kadm5_modify_principal(kadm_handle, &ent, mask);
	kadm5_free_principal_ent(kadm_handle, &ent);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	break;
    }
    case kadm_rename:{
	op = "RENAME";
	ret = krb5_ret_principal(sp, &princ);
	if(ret)
	    goto fail;
	ret = krb5_ret_principal(sp, &princ2);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	krb5_unparse_name_fixed(context->context, princ, name, sizeof(name));
	krb5_unparse_name_fixed(context->context, princ2, name2, sizeof(name2));
	krb5_warnx(context->context, "%s: %s %s -> %s", 
		   client, op, name, name2);
	ret = _kadm5_acl_check_permission(context, 
					  KADM5_PRIV_ADD|KADM5_PRIV_DELETE);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	ret = kadm5_rename_principal(kadm_handle, princ, princ2);
	krb5_free_principal(context->context, princ);
	krb5_free_principal(context->context, princ2);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	break;
    }
    case kadm_chpass:{
	op = "CHPASS";
	ret = krb5_ret_principal(sp, &princ);
	if(ret)
	    goto fail;
	ret = krb5_ret_string(sp, &password);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	krb5_unparse_name_fixed(context->context, princ, name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
#if 0
	/* anyone can change her/his own password */
	/* but not until there is a way to ensure that the
           authentication was done via an initial ticket request */
	if(!krb5_principal_compare(context->context, context->caller, princ))
	    ret = KADM5_AUTH_INSUFFICIENT;
	if(ret)
#endif
	    ret = _kadm5_acl_check_permission(context, KADM5_PRIV_CPW);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	ret = kadm5_chpass_principal(kadm_handle, princ, password);
	krb5_free_principal(context->context, princ);
	memset(password, 0, strlen(password));
	free(password);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	break;
    }
    case kadm_randkey:{
	op = "RANDKEY";
	ret = krb5_ret_principal(sp, &princ);
	if(ret)
	    goto fail;
	krb5_unparse_name_fixed(context->context, princ, name, sizeof(name));
	krb5_warnx(context->context, "%s: %s %s", client, op, name);
#if 0
	/* anyone can change her/his own password */
	/* but not until there is a way to ensure that the
           authentication was done via an initial ticket request */
	if(!krb5_principal_compare(context->context, context->caller, princ))
	    ret = KADM5_AUTH_INSUFFICIENT;
	if(ret)
#endif
	    ret = _kadm5_acl_check_permission(context, KADM5_PRIV_CPW);
	if(ret){
	    krb5_free_principal(context->context, princ);
	    goto fail;
	}
	ret = kadm5_randkey_principal(kadm_handle, princ, 
				      &new_keys, &n_keys);
	krb5_free_principal(context->context, princ);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	if(ret == 0){
	    int i;
	    krb5_store_int32(sp, n_keys);
	    for(i = 0; i < n_keys; i++){
		krb5_store_keyblock(sp, new_keys[i]);
		krb5_free_keyblock_contents(context->context, &new_keys[i]);
	    }
	}
	break;
    }
    case kadm_get_privs:{
	ret = kadm5_get_privs(kadm_handle, &mask);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	if(ret == 0)
	    krb5_store_int32(sp, mask);
	break;
    }
    case kadm_get_princs:{
	op = "LIST";
	ret = krb5_ret_int32(sp, &tmp);
	if(ret)
	    goto fail;
	if(tmp){
	    ret = krb5_ret_string(sp, &exp);
	    if(ret)
		goto fail;
	}else
	    exp = NULL;
	krb5_warnx(context->context, "%s: %s %s", client, op, exp ? exp : "*");
	ret = _kadm5_acl_check_permission(context, KADM5_PRIV_LIST);
	if(ret){
	    free(exp);
	    goto fail;
	}
	ret = kadm5_get_principals(kadm_handle, exp, &princs, &n_princs);
	free(exp);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, ret);
	if(ret == 0){
	    int i;
	    krb5_store_int32(sp, n_princs);
	    for(i = 0; i < n_princs; i++)
		krb5_store_string(sp, princs[i]);
	    kadm5_free_name_list(kadm_handle, princs, &n_princs);
	}
	break;
    }
    default:
	krb5_warnx(context->context, "%s: UNKNOWN OP %d", client, cmd);
	krb5_storage_free(sp);
	sp = krb5_storage_emem();
	krb5_store_int32(sp, KADM5_FAILURE);
	break;
    }
    krb5_storage_to_data(sp, out);
    krb5_storage_free(sp);
    return 0;
fail:
    krb5_warn(context->context, ret, "%s", op);
    sp->seek(sp, 0, SEEK_SET);
    krb5_store_int32(sp, ret);
    krb5_storage_to_data(sp, out);
    krb5_storage_free(sp);
    return 0;
}

static void
v5_loop (krb5_context context,
	 krb5_auth_context ac,
	 void *kadm_handle,
	 int fd)
{
    krb5_error_code ret;
    ssize_t n;
    unsigned long len;
    u_char tmp[4];
    struct iovec iov[2];
    krb5_data in, out, msg, reply;

    for (;;) {
	krb5_net_read(context, &fd, tmp, 4);
	_krb5_get_int (tmp, &len, 4);

	in.length = len;
	in.data = malloc(in.length);
	n = krb5_net_read(context, &fd, in.data, in.length);
	if (n == 0)
	    exit (0);
	if(n < 0)
	    krb5_errx(context, 1, "read error: %d", errno);
	if(n < in.length)
	    krb5_errx(context, 1, "short read (%ld)", (long int)n);
	ret = krb5_rd_priv(context, ac, &in, &out, NULL);
	krb5_data_free(&in);
	kadmind_dispatch(kadm_handle, &out, &msg);
	krb5_data_free(&out);
	ret = krb5_mk_priv(context, ac, &msg, &reply, NULL);
	krb5_data_free(&msg);
	if(ret) 
	    krb5_err(context, 1, ret, "krb5_mk_priv");

	_krb5_put_int(tmp, reply.length, 4);

	iov[0].iov_base = tmp;
	iov[0].iov_len  = 4;
	iov[1].iov_base = reply.data;
	iov[1].iov_len  = reply.length;
	n = writev(fd, iov, 2);
	krb5_data_free(&reply);
	if(n < 0)
	    krb5_err(context, 1, errno, "writev");
	if(n < iov[0].iov_len + iov[1].iov_len)
	    krb5_errx(context, 1, "short write");
    }
}

static void
handle_v5(krb5_context context,
	  krb5_auth_context ac,
	  krb5_keytab keytab,
	  int len,
	  int fd)
{
    krb5_error_code ret;
    u_char version[sizeof(KRB5_SENDAUTH_VERSION)];
    krb5_ticket *ticket;
    krb5_principal server;
    char *client;
    void *kadm_handle;

    if (len != sizeof(KRB5_SENDAUTH_VERSION))
	krb5_errx(context, 1, "bad sendauth len %d", len);
    if(krb5_net_read(context, &fd, version, len) != len)
	krb5_err (context, 1, errno, "reading sendauth version");
    if(memcmp(version, KRB5_SENDAUTH_VERSION, len) != 0)
	krb5_errx(context, 1, "bad sendauth version %.8s", version);
	
    ret = krb5_parse_name(context, KADM5_ADMIN_SERVICE, &server);
    if (ret)
	krb5_err (context, 1, ret, "krb5_parse_name %s", KADM5_ADMIN_SERVICE);
    ret = krb5_recvauth(context, &ac, &fd, KADMIN_APPL_VERSION, 
			server, KRB5_RECVAUTH_IGNORE_VERSION, 
			keytab, &ticket);
    krb5_free_principal(context, server);
	    
    if(ret)
	krb5_err(context, 1, ret, "krb5_recvauth");
    ret = krb5_unparse_name(context, ticket->client, &client);
    if (ret)
	krb5_err (context, 1, ret, "krb5_unparse_name");
    ret = kadm5_init_with_password_ctx(context, 
				       client, 
				       NULL,
				       KADM5_ADMIN_SERVICE,
				       NULL, 0, 0, 
				       &kadm_handle);
    if(ret)
	krb5_err (context, 1, ret, "kadm5_init_with_password_ctx");
    v5_loop (context, ac, kadm_handle, fd);
}

krb5_error_code
kadmind_loop(krb5_context context,
	     krb5_auth_context ac,
	     krb5_keytab keytab, 
	     int fd)
{
    unsigned char tmp[4];
    ssize_t n;
    unsigned long len;

    n = krb5_net_read(context, &fd, tmp, 4);
    if(n == 0)
	exit(0);
    if(n < 0)
	krb5_errx(context, 1, "read error: %d", errno);
    if(n < 4)
	krb5_errx(context, 1, "short read (%ld)", (long int)n);
    _krb5_get_int(tmp, &len, 4);
    if(len > 0xffff && (len & 0xffff) == ('K' << 8) + 'A') {
	len >>= 16;
#ifdef KRB4
	handle_v4(context, len, fd);
#else
	krb5_errx(context, 1, "packet appears to be version 4");
#endif
    } else {
	handle_v5(context, ac, keytab, len, fd);
    }
    return 0;
}
