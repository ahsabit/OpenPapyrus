/* SASL server API implementation
 * Rob Siemborski
 * Tim Martin
 */
// Copyright (c) 1998-2016 Carnegie Mellon University.  All rights reserved.
//
#include <sasl-internal.h>
#pragma hdrstop
/* sasldb stuff */
#include "saslutil.h"
#include "saslplug.h"
#include "../sasldb/sasldb.h"
#include "plugin_common.h"

static int sasldb_auxprop_lookup(void * glob_context __attribute__((unused)), sasl_server_params_t * sparams,
    uint flags, const char * user, uint ulen)
{
	char * userid = NULL;
	char * realm = NULL;
	const char * user_realm = NULL;
	int ret;
	const struct propval * to_fetch, * cur;
	char value[8192];
	size_t value_len;
	char * user_buf;
	int verify_against_hashed_password;
	int saw_user_password = 0;
	if(!sparams || !user) 
		return SASL_BADPARAM;
	user_buf = (char *)sparams->utils->FnMalloc(ulen + 1);
	if(!user_buf) {
		ret = SASL_NOMEM;
		goto done;
	}
	memcpy(user_buf, user, ulen);
	user_buf[ulen] = '\0';
	if(sparams->user_realm) {
		user_realm = sparams->user_realm;
	}
	else {
		user_realm = sparams->serverFQDN;
	}
	ret = _plug_parseuser(sparams->utils, &userid, &realm, user_realm, sparams->serverFQDN, user_buf);
	if(ret != SASL_OK) goto done;
	to_fetch = sparams->utils->prop_get(sparams->propctx);
	if(!to_fetch) {
		ret = SASL_NOMEM;
		goto done;
	}

	verify_against_hashed_password = flags & SASL_AUXPROP_VERIFY_AGAINST_HASH;

	/* Use a fake value to signal that we have no property to lookup */
	ret = SASL_CONTINUE;
	for(cur = to_fetch; cur->name; cur++) {
		int cur_ret;
		const char * realname = cur->name;

		/* Only look up properties that apply to this lookup! */
		if(cur->name[0] == '*' && (flags & SASL_AUXPROP_AUTHZID)) continue;
		if(!(flags & SASL_AUXPROP_AUTHZID)) {
			if(cur->name[0] != '*') continue;
			else realname = cur->name + 1;
		}
		/* If it's there already, we want to see if it needs to be
		 * overridden. userPassword is a special case, because it's value
		   is always present if SASL_AUXPROP_VERIFY_AGAINST_HASH is specified.
		   When SASL_AUXPROP_VERIFY_AGAINST_HASH is set, we just clear userPassword. */
		if(cur->values && !(flags & SASL_AUXPROP_OVERRIDE) && (verify_against_hashed_password == 0 || strcasecmp(realname, SASL_AUX_PASSWORD_PROP) != 0)) {
			continue;
		}
		else if(cur->values) {
			sparams->utils->prop_erase(sparams->propctx, cur->name);
		}
		if(strcasecmp(realname, SASL_AUX_PASSWORD_PROP) == 0) {
			saw_user_password = 1;
		}
		cur_ret = _sasldb_getdata(sparams->utils, sparams->utils->conn, userid, realm, realname, value, sizeof(value), &value_len);
		/* Assumption: cur_ret is never SASL_CONTINUE */

		/* If this is the first property we've tried to fetch ==>
		   always set the global error code.
		   If we had SASL_NOUSER ==> any other error code overrides it
		   (including SASL_NOUSER). */
		if(ret == SASL_CONTINUE || ret == SASL_NOUSER) {
			ret = cur_ret;
		}
		else if(ret == SASL_OK) {
			/* Any error code other than SASL_NOUSER overrides SASL_OK.
			   (And SASL_OK overrides SASL_OK as well) */
			if(cur_ret != SASL_NOUSER) {
				ret = cur_ret;
			}
		}
		/* Any other global error code is left as is */

		if(cur_ret != SASL_OK) {
			if(cur_ret != SASL_NOUSER) {
				/* No point in continuing if we hit any serious error */
				break;
			}
			/* We didn't find it, leave it as not found */
			continue;
		}

		sparams->utils->prop_set(sparams->propctx, cur->name,
		    value, (uint)value_len);
	}

	/* [Keep in sync with LDAPDB, SQL]
	   If ret is SASL_CONTINUE, it means that no properties were requested
	   (or maybe some were requested, but they already have values and
	   SASL_AUXPROP_OVERRIDE flag is not set).
	   Always return SASL_OK in this case. */
	if(ret == SASL_CONTINUE) {
		ret = SASL_OK;
	}

	if(flags & SASL_AUXPROP_AUTHZID) {
		/* This is a lie, but the caller can't handle
		   when we return SASL_NOUSER for authorization identity lookup. */
		if(ret == SASL_NOUSER) {
			ret = SASL_OK;
		}
	}
	else {
		if(ret == SASL_NOUSER && saw_user_password == 0) {
			/* Verify user existence by checking presence of
			   the userPassword attribute */
			ret = _sasldb_getdata(sparams->utils,
				sparams->utils->conn,
				userid,
				realm,
				SASL_AUX_PASSWORD_PROP,
				value,
				sizeof(value),
				&value_len);
		}
	}

done:
	if(userid) sparams->utils->FnFree(userid);
	if(realm) sparams->utils->FnFree(realm);
	if(user_buf) sparams->utils->FnFree(user_buf);

	return ret;
}

static int sasldb_auxprop_store(void * glob_context __attribute__((unused)),
    sasl_server_params_t * sparams,
    struct propctx * ctx,
    const char * user,
    uint ulen)
{
	char * userid = NULL;
	char * realm = NULL;
	const char * user_realm = NULL;
	int ret = SASL_FAIL;
	const struct propval * to_store, * cur;
	char * user_buf;

	/* just checking if we are enabled */
	if(!ctx) return SASL_OK;

	if(!sparams || !user) return SASL_BADPARAM;

	user_buf = (char *)sparams->utils->FnMalloc(ulen + 1);
	if(!user_buf) {
		ret = SASL_NOMEM;
		goto done;
	}

	memcpy(user_buf, user, ulen);
	user_buf[ulen] = '\0';

	if(sparams->user_realm) {
		user_realm = sparams->user_realm;
	}
	else {
		user_realm = sparams->serverFQDN;
	}

	ret = _plug_parseuser(sparams->utils, &userid, &realm, user_realm,
		sparams->serverFQDN, user_buf);
	if(ret != SASL_OK) goto done;

	to_store = sparams->utils->prop_get(ctx);
	if(!to_store) {
		ret = SASL_BADPARAM;
		goto done;
	}
	ret = SASL_OK;
	for(cur = to_store; cur->name; cur++) {
		const char * value = (cur->values && cur->values[0]) ? cur->values[0] : NULL;
		if(cur->name[0] == '*') {
			continue;
		}
		/* WARN: We only support one value right now. */
		ret = _sasldb_putdata(sparams->utils, sparams->utils->conn, userid, realm, cur->name, value, value ? strlen(value) : 0);
		if(value == NULL && ret == SASL_NOUSER) {
			/* Deleting something which is not there is not an error */
			ret = SASL_OK;
		}
		if(ret != SASL_OK) {
			/* We've already failed, no point in continuing */
			break;
		}
	}
done:
	if(userid) sparams->utils->FnFree(userid);
	if(realm) sparams->utils->FnFree(realm);
	if(user_buf) sparams->utils->FnFree(user_buf);

	return ret;
}

static sasl_auxprop_plug_t sasldb_auxprop_plugin = {
	0,                      /* Features */
	0,                      /* spare */
	NULL,                   /* glob_context */
	sasldb_auxprop_free,    /* auxprop_free */
	sasldb_auxprop_lookup,  /* auxprop_lookup */
	"sasldb",               /* name */
	sasldb_auxprop_store    /* auxprop_store */
};

int sasldb_auxprop_plug_init(const sasl_utils_t * utils,
    int max_version,
    int * out_version,
    sasl_auxprop_plug_t ** plug,
    const char * plugname __attribute__((unused)))
{
	if(!out_version || !plug) return SASL_BADPARAM;

	/* Do we have database support? */
	/* Note that we can use a NULL sasl_conn_t because our
	 * sasl_utils_t is "blessed" with the global callbacks */
	if(_sasl_check_db(utils, NULL) != SASL_OK)
		return SASL_NOMECH;
	/* Check if libsasl API is older than ours. If it is, fail */
	if(max_version < SASL_AUXPROP_PLUG_VERSION) return SASL_BADVERS;
	*out_version = SASL_AUXPROP_PLUG_VERSION;
	*plug = &sasldb_auxprop_plugin;
	return SASL_OK;
}
