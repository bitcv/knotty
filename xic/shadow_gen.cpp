#include "ShadowBox.h"
#include "xslib/Srp6a.h"
#include "xslib/opt.h"
#include "xslib/xbase32.h"
#include "xslib/xbase64.h"
#include "xslib/urandom.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>

#define SHADOW_GEN_VERSION	"20170516.2300"

void display_version(const char *program)
{
	fprintf(stderr, 
"shadow_gen "SHADOW_GEN_VERSION"\n"
"$build: shadow_gen-"SHADOW_GEN_VERSION" "__DATE__" "__TIME__" $\n"
	);
	exit(1);
}

void usage(const char *program)
{
	fprintf(stderr, "Usage: %s [options] <identity> [password]\n", program);
	fprintf(stderr,
"  -f filename        shadow file to read SRP6a parameters\n"
"  -p paramId         parameter id used\n"
"  -V                 display version information\n"
"\n"
"    Option -f and -p should be used both or not.\n"
"\n"
"    If identity ends with %%, the %% will be removed and replaced\n" 
"    with a random string.\n"
"\n"
"    If password is -, it will be read (at most 79 chars) from stdin.\n"
"\n"
		);
	exit(1);
}

int main(int argc, char **argv)
{
	static bset_t id_bset = make_bset_by_add_cstr(&alnum_bset, "@._-");
	static bset_t pass_bset = make_bset_by_del_cstr(&graph_bset, ":");
	const char *prog = argv[0];
	const char *identity, *password;
	const char *filename = NULL;
	const char *param = NULL;
	bool show_version = false;
	int optend;

	OPT_BEGIN(argc, argv, &optend)
	{
	case 'f':
		filename = OPT_EARG(usage(prog));
		break;
	case 'p':
		param = OPT_EARG(usage(prog));
		break;
	case 'V':
		show_version = true;
		break;
	default:
		usage(prog);
	} OPT_END();

	if (show_version)
		display_version(prog);

	if (argc - optend < 1)
		usage(prog);

	if (bool(filename) ^ bool(param))
	{
		usage(prog);
	}

	identity = argv[optend];
	password = (optend + 1 < argc) ? argv[optend + 1] : NULL;

	ShadowBoxPtr sb = ShadowBox::createFromFile(filename ? filename : "");
	xstr_t paramId = XSTR_C(param ? param : "*");

	xstr_t hashId;
	int bits;
	uintmax_t g;
	xstr_t N;
	if (!sb->getSrp6aParameter(paramId, hashId, bits, g, N))
	{
		fprintf(stderr, "Unknown paramId %s\n", param);
		exit(1);
	}

	int rc;
	bool random_id = false;
	xstr_t xs = XSTR_C(identity);

	if (xstr_equal_cstr(&xs, "%"))
	{
		random_id = true;
	}
	else if ((rc = xstr_find_in_bset(&xs, 0, &alpha_bset)) != 0)
	{
		fprintf(stderr, "ERROR: the first char of identity must be a letter\n");
		exit(1);
	}
	else if ((rc = xstr_find_not_in_bset(&xs, 0, &id_bset)) >= 0)
	{
		if (xs.data[rc] == '%' && rc == xs.len - 1)
		{
			random_id = true;
		}
		else
		{
			if (xs.data[rc] == '%')
				fprintf(stderr, "ERROR: char '%c' can only be the last char of identity", xs.data[rc]);
			else
				fprintf(stderr, "ERROR: invalid char '%c' in identity, which can only contain letters, digits and ones of \"@._-\"\n", xs.data[rc]);
			exit(1);
		}
	}

	char idbuf[80];
	if (random_id)
	{
		char *p = stpncpy(idbuf, identity, sizeof(idbuf));
		--p;
		int used = p - idbuf;
		if ((size_t)used < sizeof(idbuf) - 1)
		{
			int left = sizeof(idbuf) - used;
			if (left > 17)
				left = (used < 8) ? 25 - used : 17;
			urandom_generate_id(p, left);
		}
		idbuf[sizeof(idbuf) - 1] = 0;
		identity = idbuf;
	}

	bool random_pass = false;
	char passbuf[80];
	if (!password)
	{
		random_pass = true;
		uint8_t randbuf[25];
		urandom_get_bytes(randbuf, sizeof(randbuf));
		xbase32_encode(passbuf, randbuf, sizeof(randbuf));
		password = passbuf;
	}
	else if (strcmp(password, "-") == 0)
	{
		char line[80];
		fgets(line, sizeof(line), stdin);
		xstr_t xs = XSTR_C(line);
		xstr_trim(&xs);
		int rc = xstr_find_not_in_bset(&xs, 0, &pass_bset);
		if (rc >= 0)
		{
			fprintf(stderr, "ERROR: the password contains invalid char '%c'\n", xs.data[rc]);
			exit(1);
		}
		xstr_copy_to(&xs, passbuf, sizeof(passbuf));
		password = passbuf;
	}
	else
	{
		xstr_t xs = XSTR_C(password);
		int rc = xstr_find_not_in_bset(&xs, 0, &pass_bset);
		if (rc >= 0)
		{
			fprintf(stderr, "ERROR: the password contains invalid char '%c'\n", xs.data[rc]);
			exit(1);
		}
	}

	Srp6aClientPtr srp6a = new Srp6aClient(g, N, bits, hashId);
	srp6a->set_identity(identity, password);
	srp6a->gen_salt();
	xstr_t salt = srp6a->get_salt();
	xstr_t v = srp6a->compute_v();

	int i, len;
	char buf[8192];
	len = xbase64_encode(&url_xbase64, buf, salt.data, salt.len, XBASE64_NO_PADDING);
	fprintf(stdout, "\n[verifier]\n");
	fprintf(stdout, "\n!%s = SRP6a:%.*s:%.*s:\n", identity, XSTR_P(&paramId), len, buf);

	len = xbase64_encode(&url_xbase64, buf, v.data, v.len, XBASE64_NO_PADDING);
	for (i = 0; i < len; i += 64)
	{
		int n = len - i;
		if (n > 64)
			n = 64;

		fputs("        ", stdout);
		fwrite(buf + i, 1, n, stdout);
		fputc('\n', stdout);
	}
	fputc('\n', stdout);

	if (random_id || random_pass)
	{
		fputc('\n', stderr);
		if (random_id)
			fprintf(stderr, "## ID = %s\n", identity);
		if (random_pass)
			fprintf(stderr, "## PS = %s\n", passbuf);
		fputc('\n', stderr);
	}

	return 0;
}

