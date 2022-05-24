typedef struct {
	char	name[16];
	u_long	size;
	u_long	addr;
} QLPFILE;


int		QLPfileCount(u_long *qlp_ptr);
QLPFILE	QLPfile(u_long *qlp_ptr, int filenum);
u_long	*QLPfilePtr(u_long *qlp_ptr, int filenum);


int QLPfileCount(u_long *qlp_ptr) {
	
	return *(u_long*)(qlp_ptr + 1);
	
}

QLPFILE QLPfile(u_long *qlp_ptr, int filenum) {
	
	return *((QLPFILE*)(qlp_ptr + 2) + filenum);
	
}

u_long *QLPfilePtr(u_long *qlp_ptr, int filenum) {
	
	return (qlp_ptr + ((QLPFILE*)(qlp_ptr + 2) + filenum)->addr);
	
}