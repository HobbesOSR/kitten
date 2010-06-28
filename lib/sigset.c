#include <lwk/kernel.h>
#include <lwk/sigset.h>


#define _NSIG_WORDS	BITS_TO_LONGS(NUM_SIGNALS)
#define _NSIG_BPW	BITS_PER_LONG


#if (_NSIG_WORDS != 1) && (_NSIG_WORDS != 2) && (_NSIG_WORDS != 4)
#error "_NSIG_WORDS must be 1, 2, or 4" 
#endif


void sigset_add(sigset_t *sigset, int signum)
{
	unsigned long sig = signum - 1;
	if (_NSIG_WORDS == 1)
		sigset->bitmap[0] |= 1UL << sig;
	else
		sigset->bitmap[sig / _NSIG_BPW] |= 1UL << (sig % _NSIG_BPW);
}


void sigset_del(sigset_t *sigset, int signum)
{
	unsigned long sig = signum - 1;
	if (_NSIG_WORDS == 1)
		sigset->bitmap[0] &= ~(1UL << sig);
	else
		sigset->bitmap[sig / _NSIG_BPW] &= ~(1UL << (sig % _NSIG_BPW));
}


bool sigset_test(const sigset_t *sigset, int signum)
{
	unsigned long sig = signum - 1;
	if (_NSIG_WORDS == 1)
		return 1 & (sigset->bitmap[0] >> sig);
	else
		return 1 & (sigset->bitmap[sig / _NSIG_BPW] >> (sig % _NSIG_BPW));
}


bool sigset_isempty(const sigset_t *sigset)
{
	switch (_NSIG_WORDS) {
	case 4:
		return (sigset->bitmap[3] | sigset->bitmap[2] |
			sigset->bitmap[1] | sigset->bitmap[0]) == 0;
	case 2:
		return (sigset->bitmap[1] | sigset->bitmap[0]) == 0;
	case 1:
		return sigset->bitmap[0] == 0;
	}
}


void sigset_zero(sigset_t *sigset)
{
	switch (_NSIG_WORDS) {
	default:
		memset(sigset, 0, sizeof(sigset_t));
		break;
	case 2: sigset->bitmap[1] = 0;
	case 1:	sigset->bitmap[0] = 0;
		break;
	}
}


void sigset_fill(sigset_t *sigset)
{
	switch (_NSIG_WORDS) {
	default:
		memset(sigset, -1, sizeof(sigset_t));
		break;
	case 2: sigset->bitmap[1] = -1;
	case 1:	sigset->bitmap[0] = -1;
		break;
	}
}


void sigset_copy(sigset_t *dst, const sigset_t *src)
{
	switch (_NSIG_WORDS) {
	    case 4: dst->bitmap[3] = src->bitmap[3];
		    dst->bitmap[2] = src->bitmap[2];
	    case 2: dst->bitmap[1] = src->bitmap[1];
	    case 1: dst->bitmap[0] = src->bitmap[0];
		    break;
	}
}


void sigset_or(sigset_t *result, const sigset_t *a, const sigset_t *b)
{
	unsigned long a0, a1, a2, a3, b0, b1, b2, b3;

	switch (_NSIG_WORDS) {
	    case 4:
		a3 = a->bitmap[3]; a2 = a->bitmap[2];
		b3 = b->bitmap[3]; b2 = b->bitmap[2];
		result->bitmap[3] = a3 | b3;
		result->bitmap[2] = a2 | b2;
	    case 2:
		a1 = a->bitmap[1]; b1 = b->bitmap[1];
		result->bitmap[1] = a1 | b1;
	    case 1:
		a0 = a->bitmap[0]; b0 = b->bitmap[0];
		result->bitmap[0] = a0 | b0;
		break;
	}	
}


void sigset_and(sigset_t *result, const sigset_t *a, const sigset_t *b)
{
	unsigned long a0, a1, a2, a3, b0, b1, b2, b3;

	switch (_NSIG_WORDS) {
	    case 4:
		a3 = a->bitmap[3]; a2 = a->bitmap[2];
		b3 = b->bitmap[3]; b2 = b->bitmap[2];
		result->bitmap[3] = a3 & b3;
		result->bitmap[2] = a2 & b2;
	    case 2:
		a1 = a->bitmap[1]; b1 = b->bitmap[1];
		result->bitmap[1] = a1 & b1;
	    case 1:
		a0 = a->bitmap[0]; b0 = b->bitmap[0];
		result->bitmap[0] = a0 & b0;
		break;
	}	
}


void sigset_nand(sigset_t *result, const sigset_t *a, const sigset_t *b)
{
	unsigned long a0, a1, a2, a3, b0, b1, b2, b3;

	switch (_NSIG_WORDS) {
	    case 4:
		a3 = a->bitmap[3]; a2 = a->bitmap[2];
		b3 = b->bitmap[3]; b2 = b->bitmap[2];
		result->bitmap[3] = a3 & ~b3;
		result->bitmap[2] = a2 & ~b2;
	    case 2:
		a1 = a->bitmap[1]; b1 = b->bitmap[1];
		result->bitmap[1] = a1 & ~b1;
	    case 1:
		a0 = a->bitmap[0]; b0 = b->bitmap[0];
		result->bitmap[0] = a0 & ~b0;
		break;
	}	
}


void sigset_complement(sigset_t *sigset)
{
	switch (_NSIG_WORDS) {
	    case 4:
		sigset->bitmap[3] = ~(sigset->bitmap[3]);
		sigset->bitmap[2] = ~(sigset->bitmap[2]);
	    case 2:
		sigset->bitmap[1] = ~(sigset->bitmap[1]);
	    case 1:
		sigset->bitmap[0] = ~(sigset->bitmap[0]);
		break;
	}
}


bool sigset_haspending(sigset_t *sigset, sigset_t *blocked)
{
	unsigned long ready = 0;

	switch (_NSIG_WORDS) {
	    case 4:
		ready |= sigset->bitmap[3] &~ blocked->bitmap[3];
		ready |= sigset->bitmap[2] &~ blocked->bitmap[2];
	    case 2:
		ready |= sigset->bitmap[1] &~ blocked->bitmap[1];
	    case 1:
		ready |= sigset->bitmap[0] &~ blocked->bitmap[0];
		break;
	}

	return ready != 0;
}


int sigset_getnext(sigset_t *sigset, sigset_t *blocked)
{
	unsigned long i, *s, *m, x;
	int signum = 0;

	s = sigset->bitmap;
	m = blocked->bitmap;
	switch (_NSIG_WORDS) {
	default:
		for (i = 0; i < _NSIG_WORDS; ++i, ++s, ++m) {
			if ((x = *s &~ *m) != 0) {
				signum = ffz(~x) + i*_NSIG_BPW + 1;
				break;
			}
		}
		break;

	case 2:
		if ((x = s[0] &~ m[0]) != 0)
			signum = 1;
		else if ((x = s[1] &~ m[1]) != 0)
			signum = _NSIG_BPW + 1;
		else
			break;
		signum += ffz(~x);
		break;

        case 1:
		if ((x = *s &~ *m) != 0)
			signum = ffz(~x) + 1;
		break;
	}

	return signum;
}
