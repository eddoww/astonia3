/*

$Id: error.h,v 1.1 2005/09/24 09:55:48 ssim Exp $

$Log: error.h,v $
Revision 1.1  2005/09/24 09:55:48  ssim
Initial revision

Revision 1.2  2003/10/13 14:12:12  sam
Added RCS tags


*/

extern int error;

char *get_error_string(int err);

#define ERR_NONE			0
#define ERR_ILLEGAL_COORDS		1
#define ERR_BLOCKED			2
#define ERR_ILLEGAL_CHARNO		3
#define ERR_ILLEGAL_DIR			4
#define ERR_CONFUSED			5
#define ERR_NO_ITEM			6
#define ERR_NOT_TAKEABLE		7
#define ERR_HAVE_CITEM			8
#define ERR_NO_CITEM			9
#define ERR_HAVE_ITEM			10
#define ERR_ILLEGAL_INVPOS		11
#define ERR_REQUIREMENTS		12
#define ERR_NO_CHAR			13
#define ERR_ILLEGAL_ATTACK		14
#define ERR_ILLEGAL_ITEMNO		15
#define ERR_DEAD			16
#define ERR_MANA_LOW			17
#define ERR_SELF			18
#define ERR_ILLEGAL_HURT		19
#define ERR_NOT_VISIBLE			20
#define ERR_UNCONCIOUS			21
#define ERR_UNKNOWN_SPELL		22
#define ERR_NOT_USEABLE			23
#define ERR_NOT_BODY			24
#define ERR_UNKNOWN_SKILL		25
#define ERR_ILLEGAL_POS			26
#define ERR_NOT_CONTAINER		27
#define ERR_ALREADY_WORKING		28
#define ERR_ILLEGAL_STORENO		29
#define ERR_ILLEGAL_STOREPOS		30
#define ERR_SOLD_OUT			31
#define ERR_GOLD_LOW			32
#define ERR_QUESTITEM			33
#define ERR_ACCESS_DENIED		34
#define ERR_NOT_IDLE			35
#define ERR_NOT_PLAYER			36
#define ERR_NO_EFFECT			37
#define ERR_ALREADY_THERE		38
