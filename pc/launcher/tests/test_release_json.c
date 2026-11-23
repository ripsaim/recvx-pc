#include <stdio.h>
#include <string.h>
#include "../release_json.h"

static int failures;
#define CHECK(name, cond) do { if (!(cond)) { fprintf(stderr,"FAIL: %s\n",name); failures++; } else printf("PASS: %s\n",name); } while(0)
static int accepts(const char *s) { RecvxRelease r; return recvx_parse_release(s,strlen(s),&r); }

int main(void)
{
    RecvxRelease r;
    const char *stable="{\"tag_name\":\"v1.12.3\",\"draft\":false,\"prerelease\":false,\"body\":\"release\"}";
    const char *fake="{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"body\":\"text says \\\"draft\\\": false and \\\"prerelease\\\": true\"}";
    const char *nested="{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"extra\":{\"draft\":true,\"tag_name\":\"bad\"}}";
    const char *escaped="{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"body\":\"quote: \\\" slash: \\\\ end\"}";
    const char *unicode="{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"body\":\"caf\\u00e9 \\ud83d\\ude00\"}";
    CHECK("valid stable release",recvx_parse_release(stable,strlen(stable),&r)&&!strcmp(r.tag,"v1.12.3")&&!strcmp(r.body,"release"));
    CHECK("reject draft",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":true,\"prerelease\":false}"));
    CHECK("reject prerelease",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":true}"));
    CHECK("notes cannot spoof stable flags",recvx_parse_release(fake,strlen(fake),&r)&&strstr(r.body,"draft")!=NULL);
    CHECK("nested metadata ignored",recvx_parse_release(nested,strlen(nested),&r)&&!strcmp(r.tag,"v2.0.0"));
    CHECK("escaped quote and slash",recvx_parse_release(escaped,strlen(escaped),&r)&&!strcmp(r.body,"quote: \"" " slash: " "\\" " end"));
    CHECK("unicode escapes decode to UTF-8",recvx_parse_release(unicode,strlen(unicode),&r)&&!strcmp(r.body,"caf\xc3\xa9 \xf0\x9f\x98\x80"));
    CHECK("reject duplicate draft",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"draft\":false,\"prerelease\":false}"));
    CHECK("reject duplicate prerelease",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"prerelease\":false}"));
    CHECK("reject malformed JSON",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,}"));
    CHECK("reject truncated JSON",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false"));
    CHECK("reject missing tag",!accepts("{\"draft\":false,\"prerelease\":false}"));
    CHECK("reject invalid required field type",!accepts("{\"tag_name\":4,\"draft\":false,\"prerelease\":false}"));
    CHECK("reject duplicate security field",!accepts("{\"tag_name\":\"v2.0.0\",\"draft\":false,\"prerelease\":false,\"draft\":true}"));
    CHECK("reject oversized response",!recvx_parse_release(stable,RECVX_JSON_MAX_BYTES+1,&r));
    CHECK("reject overlong tag",!accepts("{\"tag_name\":\"v123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890\",\"draft\":false,\"prerelease\":false}"));
    return failures?1:0;
}
