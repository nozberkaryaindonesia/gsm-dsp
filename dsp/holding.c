/* 
 * Peak detect
*/
int peak_detect16(struct cxvec *restrict in, struct vec_peak *restrict peak)
{
	int i;
	int m = INTERP_FILT_M;
	short max_idx, early_pow, late_pow;

	/* Find the power */
	vec_power(in, &pow_vec);

	if ((in->len % 16) == 0) {
		peak->gain = DSP_maxval(pow_vec.data, in->len);
		max_idx = DSP_maxidx((short *) pow_vec.data, pow_vec.len);
	} else {
		peak->gain = maxval(pow_vec.data, in->len);
		max_idx = maxidx((short *) pow_vec.data, pow_vec.len);
	}

	/* Drop back one sample so we do early / late  in the same pass */
	for (i = 0; i < m; i++) {
		interp_pt(&pow_vec, max_idx - 1, i, &early[i], &late[i]);
	}

	peak->orig = max_idx;
	if ((INTERP_FILT_M % 16) == 0) {
		early_pow = DSP_maxval(early, INTERP_FILT_M);
		late_pow = DSP_maxval(late, INTERP_FILT_M);
	} else {
		early_pow = maxval(early, INTERP_FILT_M);
		late_pow = maxval(late, INTERP_FILT_M);
	}

	/*
	 * Late sample bank includes the centre sample
	 * Keep them discrete for now to avoid confusion
	 */
	if ((early_pow > late_pow) && (early_pow > peak->gain)) {
		peak->gain = early_pow;
		peak->whole = max_idx - 1;
		peak->frac = DSP_maxidx(early, INTERP_FILT_M);
	} else if ((late_pow > early_pow) && (late_pow > peak->gain)) {
		peak->gain = late_pow;
		peak->whole = max_idx;
		peak->frac = DSP_maxidx(late, INTERP_FILT_M);
	} else {
		peak->whole = max_idx;
		peak->frac = 0; 
	}

	return 0; 
}



/* Consider the case of vector roll off and zero values are included */
/* Single sided width ignoring the adjacent two samples */
/* Total must be a factor of 2 */
/* Single sided width must be '4' */
int peak_to_mean(struct cxvec * vec, int peak, int idx, int width)
{
	int i;
	int sum = 0;

	for (i = 2; i <= (width + 1); i++) {
		sum += norm2(vec->data[idx - i]);
		sum += norm2(vec->data[idx + i]);
	}

	/* For 8 samples */
	/* Not square rooting this value so we need to make power comparisons */
	return ((peak >> 2) > (sum >> 3));
}
