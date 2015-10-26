
#line 1 "src/config_parser.rl"
//
//  config_parser.rl
//

#include <cstdio>
#include <cassert>
#include <cstdlib>
#include <ctype.h>
#include <cstring>

#include "config_parser.h"


#line 54 "src/config_parser.rl"



#line 21 "src/config_parser.cc"
static const char _config_parser_actions[] = {
	0, 1, 0, 1, 1, 1, 2, 1, 
	3, 1, 4, 1, 5, 1, 6, 2, 
	1, 0, 2, 1, 6, 2, 2, 0, 
	2, 2, 6, 2, 5, 0, 2, 5, 
	6
};

static const char _config_parser_key_offsets[] = {
	0, 0, 15, 30, 32, 37, 38, 40, 
	40, 40, 40, 57, 74, 91
};

static const char _config_parser_trans_keys[] = {
	13, 32, 33, 59, 92, 124, 126, 9, 
	10, 36, 38, 40, 95, 97, 122, 13, 
	32, 33, 34, 39, 59, 92, 123, 126, 
	9, 10, 36, 95, 97, 124, 34, 92, 
	13, 32, 59, 9, 10, 10, 39, 92, 
	13, 32, 34, 35, 39, 92, 125, 9, 
	10, 33, 58, 60, 95, 97, 122, 124, 
	126, 13, 32, 34, 35, 39, 92, 125, 
	9, 10, 33, 58, 60, 95, 97, 122, 
	124, 126, 13, 32, 34, 35, 39, 92, 
	125, 9, 10, 33, 58, 60, 95, 97, 
	122, 124, 126, 13, 32, 34, 35, 39, 
	92, 125, 9, 10, 33, 58, 60, 95, 
	97, 122, 124, 126, 0
};

static const char _config_parser_single_lengths[] = {
	0, 7, 9, 2, 3, 1, 2, 0, 
	0, 0, 7, 7, 7, 7
};

static const char _config_parser_range_lengths[] = {
	0, 4, 3, 0, 1, 0, 0, 0, 
	0, 0, 5, 5, 5, 5
};

static const char _config_parser_index_offsets[] = {
	0, 0, 12, 25, 28, 33, 35, 38, 
	39, 40, 41, 54, 67, 80
};

static const char _config_parser_indicies[] = {
	0, 0, 2, 3, 4, 2, 2, 0, 
	2, 2, 2, 1, 5, 5, 6, 7, 
	8, 9, 10, 11, 6, 5, 6, 6, 
	1, 13, 14, 12, 15, 15, 16, 15, 
	1, 18, 17, 13, 20, 19, 19, 2, 
	12, 18, 18, 7, 17, 8, 10, 21, 
	18, 6, 6, 6, 6, 1, 22, 22, 
	24, 25, 26, 27, 28, 22, 23, 23, 
	23, 23, 1, 29, 29, 31, 32, 33, 
	34, 35, 29, 30, 30, 30, 30, 1, 
	36, 36, 38, 39, 40, 41, 42, 36, 
	37, 37, 37, 37, 1, 0
};

static const char _config_parser_trans_targs[] = {
	2, 0, 1, 11, 8, 2, 1, 3, 
	6, 11, 8, 13, 3, 4, 9, 2, 
	11, 5, 10, 6, 7, 12, 10, 1, 
	3, 5, 6, 8, 12, 10, 1, 3, 
	5, 6, 8, 12, 10, 1, 3, 5, 
	6, 8, 12
};

static const char _config_parser_trans_actions[] = {
	7, 0, 0, 7, 0, 0, 1, 1, 
	1, 0, 1, 0, 0, 0, 0, 9, 
	9, 0, 0, 0, 0, 0, 11, 27, 
	27, 11, 27, 27, 11, 5, 21, 21, 
	5, 21, 21, 5, 3, 15, 15, 3, 
	15, 15, 3
};

static const char _config_parser_eof_actions[] = {
	0, 0, 0, 0, 0, 0, 0, 0, 
	0, 0, 13, 30, 24, 18
};

static const int config_parser_start = 10;
static const int config_parser_first_final = 10;
static const int config_parser_error = 0;

static const int config_parser_en_main = 10;


#line 57 "src/config_parser.rl"

bool config_parser::parse(const char *buffer, size_t len)
{
    int cs = config_parser_en_main;
    
    const char *mark = NULL;
    const char *p = buffer;
    const char *pe = buffer + strlen(buffer);
    const char *eof = pe;

    
#line 124 "src/config_parser.cc"
	{
	cs = config_parser_start;
	}

#line 68 "src/config_parser.rl"
    
#line 131 "src/config_parser.cc"
	{
	int _klen;
	unsigned int _trans;
	const char *_acts;
	unsigned int _nacts;
	const char *_keys;

	if ( p == pe )
		goto _test_eof;
	if ( cs == 0 )
		goto _out;
_resume:
	_keys = _config_parser_trans_keys + _config_parser_key_offsets[cs];
	_trans = _config_parser_index_offsets[cs];

	_klen = _config_parser_single_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + _klen - 1;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + ((_upper-_lower) >> 1);
			if ( (*p) < *_mid )
				_upper = _mid - 1;
			else if ( (*p) > *_mid )
				_lower = _mid + 1;
			else {
				_trans += (unsigned int)(_mid - _keys);
				goto _match;
			}
		}
		_keys += _klen;
		_trans += _klen;
	}

	_klen = _config_parser_range_lengths[cs];
	if ( _klen > 0 ) {
		const char *_lower = _keys;
		const char *_mid;
		const char *_upper = _keys + (_klen<<1) - 2;
		while (1) {
			if ( _upper < _lower )
				break;

			_mid = _lower + (((_upper-_lower) >> 1) & ~1);
			if ( (*p) < _mid[0] )
				_upper = _mid - 2;
			else if ( (*p) > _mid[1] )
				_lower = _mid + 2;
			else {
				_trans += (unsigned int)((_mid - _keys)>>1);
				goto _match;
			}
		}
		_trans += _klen;
	}

_match:
	_trans = _config_parser_indicies[_trans];
	cs = _config_parser_trans_targs[_trans];

	if ( _config_parser_trans_actions[_trans] == 0 )
		goto _again;

	_acts = _config_parser_actions + _config_parser_trans_actions[_trans];
	_nacts = (unsigned int) *_acts++;
	while ( _nacts-- > 0 )
	{
		switch ( *_acts++ )
		{
	case 0:
#line 17 "src/config_parser.rl"
	{ mark = p; }
	break;
	case 1:
#line 18 "src/config_parser.rl"
	{ start_block(); }
	break;
	case 2:
#line 19 "src/config_parser.rl"
	{ end_block(); }
	break;
	case 3:
#line 20 "src/config_parser.rl"
	{ symbol(mark, p - mark); }
	break;
	case 4:
#line 21 "src/config_parser.rl"
	{ symbol(mark + 1, p - mark - 2); }
	break;
	case 5:
#line 22 "src/config_parser.rl"
	{ end_statement(); }
	break;
#line 229 "src/config_parser.cc"
		}
	}

_again:
	if ( cs == 0 )
		goto _out;
	if ( ++p != pe )
		goto _resume;
	_test_eof: {}
	if ( p == eof )
	{
	const char *__acts = _config_parser_actions + _config_parser_eof_actions[cs];
	unsigned int __nacts = (unsigned int) *__acts++;
	while ( __nacts-- > 0 ) {
		switch ( *__acts++ ) {
	case 1:
#line 18 "src/config_parser.rl"
	{ start_block(); }
	break;
	case 2:
#line 19 "src/config_parser.rl"
	{ end_block(); }
	break;
	case 5:
#line 22 "src/config_parser.rl"
	{ end_statement(); }
	break;
	case 6:
#line 24 "src/config_parser.rl"
	{ 
        config_done();
        {p++; goto _out; }
    }
	break;
#line 264 "src/config_parser.cc"
		}
	}
	}

	_out: {}
	}

#line 69 "src/config_parser.rl"

    return (cs != config_parser_error && cs == config_parser_first_final);
}
