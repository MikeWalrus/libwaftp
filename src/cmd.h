#ifndef _CMD_H
#define _CMD_H

enum ReplyCode1 {
	POS_PRE = 1, /// Positive Preliminary Reply
	POS_COM = 2, /// Positive Completion Reply
	POS_INT = 3, /// Positive Intermediate Reply
	NEG_TRAN_COM = 4, /// Transient Negative Reply
	NEG_PERM_COM = 5, /// Permanet Negative Reply
};

enum ReplyCode2 {
	SYNTAX = 0,
	INFORMATION = 1,
	CONNECTIONS = 2,
	AUTH_ACCOUNTING = 3,
	FILE_SYSTEM = 5
};

struct Reply {
	union {
		struct {
			enum ReplyCode1 first;
			enum ReplyCode2 second;
			unsigned int third;
		};
		unsigned int reply_codes[3];
	};
};

#endif
