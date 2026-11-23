/* Small strict JSON reader for GitHub release metadata. Header-only, no runtime dependency. */
#ifndef RECVX_RELEASE_JSON_H
#define RECVX_RELEASE_JSON_H
#include <stddef.h>
#include <string.h>

#define RECVX_JSON_MAX_BYTES (1024u * 1024u)
#define RECVX_JSON_MAX_DEPTH 64
#define RECVX_JSON_TAG_CAP 96
#define RECVX_JSON_BODY_CAP 8192
typedef struct { char tag[RECVX_JSON_TAG_CAP]; char body[RECVX_JSON_BODY_CAP]; int draft, prerelease; } RecvxRelease;
typedef struct { const unsigned char *s; size_t n, i; int depth; } RecvxJson;

static void rj_ws(RecvxJson *p) { while(p->i<p->n && (p->s[p->i]==' '||p->s[p->i]=='\t'||p->s[p->i]=='\r'||p->s[p->i]=='\n')) p->i++; }
static int rj_hex(unsigned char c) { if(c>='0'&&c<='9')return c-'0'; if(c>='a'&&c<='f')return c-'a'+10; if(c>='A'&&c<='F')return c-'A'+10; return -1; }
static int rj_u16(RecvxJson *p, unsigned *v) { int k,h; unsigned x=0; for(k=0;k<4;k++){if(p->i>=p->n||(h=rj_hex(p->s[p->i++]))<0)return 0;x=(x<<4)|(unsigned)h;}*v=x;return 1; }
static int rj_emit(char *out,size_t cap,size_t *used,unsigned cp)
{
    unsigned char b[4];size_t n;
    if(cp<=0x7f){b[0]=(unsigned char)cp;n=1;}else if(cp<=0x7ff){b[0]=(unsigned char)(0xc0|(cp>>6));b[1]=(unsigned char)(0x80|(cp&63));n=2;}
    else if(cp<=0xffff){if(cp>=0xd800&&cp<=0xdfff)return 0;b[0]=(unsigned char)(0xe0|(cp>>12));b[1]=(unsigned char)(0x80|((cp>>6)&63));b[2]=(unsigned char)(0x80|(cp&63));n=3;}
    else if(cp<=0x10ffff){b[0]=(unsigned char)(0xf0|(cp>>18));b[1]=(unsigned char)(0x80|((cp>>12)&63));b[2]=(unsigned char)(0x80|((cp>>6)&63));b[3]=(unsigned char)(0x80|(cp&63));n=4;}else return 0;
    if(out){if(*used+n>=cap)return 0;memcpy(out+*used,b,n);}*used+=n;return 1;
}
static int rj_string(RecvxJson *p,char *out,size_t cap)
{
    size_t used=0; if(p->i>=p->n||p->s[p->i++]!='"')return 0;
    while(p->i<p->n){unsigned c=p->s[p->i++];if(c=='"'){if(out)out[used]=0;return 1;}if(c<0x20)return 0;
        if(c=='\\'){unsigned cp,u;if(p->i>=p->n)return 0;c=p->s[p->i++];switch(c){case '"':case '\\':case '/':cp=c;break;case 'b':cp=8;break;case 'f':cp=12;break;case 'n':cp=10;break;case 'r':cp=13;break;case 't':cp=9;break;case 'u':
            if(!rj_u16(p,&cp))return 0;if(cp>=0xd800&&cp<=0xdbff){if(p->n-p->i<6||p->s[p->i++]!='\\'||p->s[p->i++]!='u'||!rj_u16(p,&u)||u<0xdc00||u>0xdfff)return 0;cp=0x10000+((cp-0xd800)<<10)+(u-0xdc00);}else if(cp>=0xdc00&&cp<=0xdfff)return 0;break;default:return 0;}if(!rj_emit(out,cap,&used,cp))return 0;
        }else{
            unsigned cp=c,need,j;if(c<0x80){if(!rj_emit(out,cap,&used,cp))return 0;continue;}
            if(c>=0xc2&&c<=0xdf){need=1;cp=c&31;}else if(c>=0xe0&&c<=0xef){need=2;cp=c&15;}else if(c>=0xf0&&c<=0xf4){need=3;cp=c&7;}else return 0;
            if(p->n-p->i<need)return 0;for(j=0;j<need;j++){unsigned t=p->s[p->i++];if((t&0xc0)!=0x80)return 0;if(j==0&&((c==0xe0&&t<0xa0)||(c==0xed&&t>=0xa0)||(c==0xf0&&t<0x90)||(c==0xf4&&t>=0x90)))return 0;cp=(cp<<6)|(t&63);}if(!rj_emit(out,cap,&used,cp))return 0;
        }
    }return 0;
}
static int rj_value(RecvxJson *p);
static int rj_object(RecvxJson *p)
{
    if(++p->depth>RECVX_JSON_MAX_DEPTH)return 0;p->i++;rj_ws(p);if(p->i<p->n&&p->s[p->i]=='}'){p->i++;p->depth--;return 1;}
    for(;;){if(!rj_string(p,NULL,0))return 0;rj_ws(p);if(p->i>=p->n||p->s[p->i++]!=':')return 0;rj_ws(p);if(!rj_value(p))return 0;rj_ws(p);if(p->i>=p->n)return 0;if(p->s[p->i]=='}'){p->i++;p->depth--;return 1;}if(p->s[p->i++]!=',')return 0;rj_ws(p);}
}
static int rj_array(RecvxJson *p)
{
    if(++p->depth>RECVX_JSON_MAX_DEPTH)return 0;p->i++;rj_ws(p);if(p->i<p->n&&p->s[p->i]==']'){p->i++;p->depth--;return 1;}
    for(;;){if(!rj_value(p))return 0;rj_ws(p);if(p->i>=p->n)return 0;if(p->s[p->i]==']'){p->i++;p->depth--;return 1;}if(p->s[p->i++]!=',')return 0;rj_ws(p);}
}
static int rj_number(RecvxJson *p)
{
    if(p->s[p->i]=='-')p->i++;if(p->i>=p->n)return 0;if(p->s[p->i]=='0')p->i++;else{if(p->s[p->i]<'1'||p->s[p->i]>'9')return 0;while(p->i<p->n&&p->s[p->i]>='0'&&p->s[p->i]<='9')p->i++;}
    if(p->i<p->n&&p->s[p->i]=='.'){p->i++;if(p->i>=p->n||p->s[p->i]<'0'||p->s[p->i]>'9')return 0;while(p->i<p->n&&p->s[p->i]>='0'&&p->s[p->i]<='9')p->i++;}
    if(p->i<p->n&&(p->s[p->i]=='e'||p->s[p->i]=='E')){p->i++;if(p->i<p->n&&(p->s[p->i]=='+'||p->s[p->i]=='-'))p->i++;if(p->i>=p->n||p->s[p->i]<'0'||p->s[p->i]>'9')return 0;while(p->i<p->n&&p->s[p->i]>='0'&&p->s[p->i]<='9')p->i++;}return 1;
}
static int rj_value(RecvxJson *p)
{
    if(p->i>=p->n)return 0;switch(p->s[p->i]){case '"':return rj_string(p,NULL,0);case '{':return rj_object(p);case '[':return rj_array(p);case 't':if(p->n-p->i>=4&&!memcmp(p->s+p->i,"true",4)){p->i+=4;return 1;}return 0;case 'f':if(p->n-p->i>=5&&!memcmp(p->s+p->i,"false",5)){p->i+=5;return 1;}return 0;case 'n':if(p->n-p->i>=4&&!memcmp(p->s+p->i,"null",4)){p->i+=4;return 1;}return 0;default:if(p->s[p->i]=='-'||(p->s[p->i]>='0'&&p->s[p->i]<='9'))return rj_number(p);return 0;}
}
static int recvx_parse_release(const char *json,size_t len,RecvxRelease *out)
{
    RecvxJson p;unsigned seen=0;char key[64],tmp[RECVX_JSON_BODY_CAP];int b;
    if(!json||!out||!len||len>RECVX_JSON_MAX_BYTES)return 0;memset(out,0,sizeof(*out));p.s=(const unsigned char*)json;p.n=len;p.i=0;p.depth=0;rj_ws(&p);if(p.i>=p.n||p.s[p.i++]!='{')return 0;rj_ws(&p);
    if(p.i<p.n&&p.s[p.i]=='}')return 0;
    for(;;){unsigned bit=0;if(!rj_string(&p,key,sizeof(key)))return 0;rj_ws(&p);if(p.i>=p.n||p.s[p.i++]!=':')return 0;rj_ws(&p);
        if(!strcmp(key,"tag_name"))bit=1;else if(!strcmp(key,"draft"))bit=2;else if(!strcmp(key,"prerelease"))bit=4;else if(!strcmp(key,"body"))bit=8;
        if(bit&&(seen&bit))return 0;
        if(bit==1){if(!rj_string(&p,out->tag,sizeof(out->tag)))return 0;}
        else if(bit==8){if(!rj_string(&p,out->body,sizeof(out->body)))return 0;}
        else if(bit==2||bit==4){if(p.i>=p.n||(p.s[p.i]!='t'&&p.s[p.i]!='f'))return 0;b=(p.s[p.i]=='t');if(!rj_value(&p))return 0;if(bit==2)out->draft=b;else out->prerelease=b;}
        else if(!rj_value(&p))return 0;seen|=bit;rj_ws(&p);if(p.i>=p.n)return 0;if(p.s[p.i]=='}'){p.i++;break;}if(p.s[p.i++]!=',')return 0;rj_ws(&p);
    }
    rj_ws(&p);if(p.i!=p.n||(seen&7)!=7||!out->tag[0]||out->draft||out->prerelease)return 0;
    (void)tmp;return 1;
}
#endif
