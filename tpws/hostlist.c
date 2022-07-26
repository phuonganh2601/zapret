#include <stdio.h>
#include "hostlist.h"
#include "gzip.h"
#include "params.h"


static bool addpool(strpool **hostlist, char **s, const char *end)
{
	char *p;
	
	// advance until eol lowering all chars
	for (p = *s; p<end && *p && *p!='\r' && *p != '\n'; p++) *p=tolower(*p);
	if (!StrPoolAddStrLen(hostlist, *s, p-*s))
	{
		StrPoolDestroy(hostlist);
		*hostlist = NULL;
		return false;
	}
	// advance to the next line
	for (; p<end && (!*p || *p=='\r' || *p=='\n') ; p++);
	*s = p;
	return true;
}


bool AppendHostList(strpool **hostlist, char *filename)
{
	char *p, *e, s[256], *zbuf;
	size_t zsize;
	int ct = 0;
	FILE *F;
	int r;

	printf("Loading hostlist %s\n",filename);

	if (!(F = fopen(filename, "rb")))
	{
		fprintf(stderr, "Could not open %s\n", filename);
		return false;
	}

	if (is_gzip(F))
	{
		r = z_readfile(F,&zbuf,&zsize);
		fclose(F);
		if (r==Z_OK)
		{
			printf("zlib compression detected. uncompressed size : %zu\n", zsize);
			
			p = zbuf;
			e = zbuf + zsize;
			while(p<e)
			{
				if (!addpool(hostlist,&p,e))
				{
					fprintf(stderr, "Not enough memory to store host list : %s\n", filename);
					free(zbuf);
					return false;
				}
				ct++;
			}
			free(zbuf);
		}
		else
		{
			fprintf(stderr, "zlib decompression failed : result %d\n",r);
			return false;
		}
	}
	else
	{
		printf("loading plain text list\n");
		
		while (fgets(s, 256, F))
		{
			p = s;
			if (!addpool(hostlist,&p,p+strlen(p)))
			{
				fprintf(stderr, "Not enough memory to store host list : %s\n", filename);
				fclose(F);
				return false;
			}
			ct++;
		}
		fclose(F);
	}

	printf("Loaded %d hosts from %s\n", ct, filename);
	return true;
}

bool LoadHostLists(strpool **hostlist, struct str_list_head *file_list)
{
	struct str_list *file;

	if (*hostlist)
	{
		StrPoolDestroy(hostlist);
		*hostlist = NULL;
	}

	LIST_FOREACH(file, file_list, next)
	{
		if (!AppendHostList(hostlist, file->str)) return false;
	}
	return true;
}


bool SearchHostList(strpool *hostlist, const char *host)
{
	if (hostlist)
	{
		const char *p = host;
		bool bInHostList;
		while (p)
		{
			bInHostList = StrPoolCheckStr(hostlist, p);
			if (params.debug) printf("Hostlist check for %s : %s\n", p, bInHostList ? "positive" : "negative");
			if (bInHostList) return true;
			p = strchr(p, '.');
			if (p) p++;
		}
	}
	return false;
}

// return : true = apply fooling, false = do not apply
bool HostlistCheck(strpool *hostlist, strpool *hostlist_exclude, const char *host)
{
	if (hostlist_exclude)
	{
		if (params.debug) printf("Checking exclude hostlist\n");
		if (SearchHostList(hostlist_exclude, host)) return false;
	}
	if (hostlist)
	{
		if (params.debug) printf("Checking include hostlist\n");
		return SearchHostList(hostlist, host);
	}
	return true;
}
