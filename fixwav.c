#include <stdio.h>
#include <unistd.h>
#include "byteswap.h"
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <sndfile.h>
#include "log.h"

#define BLOCK_SIZE 65536

typedef unsigned int DWORD; 
typedef unsigned char BYTE; 
typedef DWORD FOURCC;    // Four-character code 
int doFix=0;

struct riff_chunk {
				FOURCC chunk_id;
				DWORD start_offset;
				DWORD  actual_length;
				DWORD reported_length;

				DWORD data_offset;

				int active;
				int present;
				int error;
				uint chunkOrder;

				struct riff_chunk * subchunks;
				struct riff_chunk * next;
				struct riff_chunk * prior;
};

struct riff_chunk * chunklist=NULL;
struct riff_chunk * active_chunk=NULL;

struct riff_chunk * detectChunkBoundary(struct riff_chunk * rc, FOURCC * bf) {

				do { 
								if (rc && (rc->chunk_id==(*bf))) 
												return rc;
								else if (rc && (rc->subchunks))
												return (detectChunkBoundary(rc->subchunks,bf));

				} while (rc,rc=rc->next,rc!=NULL);
				return NULL;
}

void clearActive(struct riff_chunk * rc) {
				if (rc)
								rc->active=0;
}

void incrementDataLength8(struct riff_chunk * rc) {
				if (rc && rc->active)
								rc->actual_length+=8;
}

void incrementDataLength(struct riff_chunk * rc) {
				if (rc && rc->active)
								rc->actual_length++;
}

void checkChunk(struct riff_chunk * rc) {
				if (rc && (rc->reported_length != rc->actual_length)){
								rc->error=1;
				}
}

/*
 * print details for this chunk
 */
void printChunk(struct riff_chunk * rc) {
				if (rc && rc->present)
								if (rc->error)
												log_error("chunk %c%c%c%c, start=%u, length=%u, reported_length=%u, present=%u, chunkOrder=%u",
																				(rc->chunk_id >> 0) & 0x000000ff,
																				(rc->chunk_id >> 8) & 0x000000ff,
																				(rc->chunk_id >> 16) & 0x000000ff,
																				(rc->chunk_id >> 24) & 0x000000ff,
																				rc->start_offset,rc->actual_length,rc->reported_length, rc->present,rc->chunkOrder);
								else
												log_info("chunk %c%c%c%c, start=%u, length=%u, reported_length=%u, present=%u, chunkOrder=%u",
																				(rc->chunk_id >> 0) & 0x000000ff,
																				(rc->chunk_id >> 8) & 0x000000ff,
																				(rc->chunk_id >> 16) & 0x000000ff,
																				(rc->chunk_id >> 24) & 0x000000ff,
																				rc->start_offset,rc->actual_length,rc->reported_length, rc->present,rc->chunkOrder);
}

void chunkReadWrite(struct riff_chunk * rc,FILE * pFileIn, FILE * pFileOut) {
				if (rc && rc->present) {
								unsigned char * cbuf=calloc(sizeof(unsigned char),4096);
								fwrite(&rc->chunk_id,sizeof(FOURCC),1,pFileOut);
								fwrite(&rc->actual_length,sizeof(DWORD),1,pFileOut);
								fseek(pFileIn,rc->start_offset + 8,SEEK_SET);

								DWORD dcount=0;
								DWORD fcount=0;
								DWORD rcount=0;

								if (rc->chunk_id == bswap_32((DWORD)'RIFF') ){
												dcount=4;
								} else {
												dcount=rc->actual_length;
								}

								while (dcount) {
												if (dcount>4096)
																rcount=4096;
												else
																rcount=dcount;
												fcount=fread(cbuf,sizeof(unsigned char),rcount,pFileIn);
												dcount-=fcount;
												fwrite(cbuf,sizeof(unsigned char),fcount,pFileOut);
								}
				}
}

/*
 * this function iterates recursively through all possible chunks
 */
void transferChunks(struct riff_chunk * rc,void (*callback)(struct riff_chunk *,FILE *, FILE * ),FILE * pFileIn,FILE * pFileOut) {
				do { 
								if (rc)
												(*callback)(rc,pFileIn,pFileOut);
								if (rc && (rc->subchunks))
												transferChunks(rc->subchunks,callback,pFileIn,pFileOut);
				} while (rc,rc=rc->next,rc!=NULL);
}
/*
 * this function iterates recursively through all possible chunks
 */
void iterateChunks(struct riff_chunk * rc,void (*callback)(struct riff_chunk * )) {
				do { 
								if (rc)
												(*callback)(rc);
								if (rc && (rc->subchunks))
												iterateChunks(rc->subchunks,callback);
				} while (rc,rc=rc->next,rc!=NULL);
}

/*
 * return a pointer to the first chunk in this branch
 */
struct riff_chunk * getFirstChunk(struct riff_chunk * rc) {
				while(1) {
								if (rc->prior)
												rc=rc->prior;
								else
												break;
				} 
				return rc;
}

struct riff_chunk * getLastChunk(struct riff_chunk * rc) {
				do { if (rc && rc->next==NULL) 
								return rc;
								if (!rc) break;
								rc=rc->next;
				} while (rc!=NULL);
				return NULL;
}

struct riff_chunk * addChunk(struct riff_chunk ** prcp, FOURCC cid) {

				struct riff_chunk * rcp=getLastChunk(*prcp);

				struct riff_chunk * rc=calloc(sizeof(struct riff_chunk),1);

				rc->chunk_id=bswap_32(cid);
				rc->subchunks=NULL;
				rc->next=NULL;
				rc->prior=NULL;
				rc->start_offset=0;
				rc->actual_length=0;
				rc->reported_length=0;
				rc->present=0;
				rc->chunkOrder=0;

				if (0)
								log_info("adding chunk %c%c%c%c",
																(cid >> 24) & 0x000000ff,
																(cid >> 16) & 0x000000ff,
																(cid >> 8) & 0x000000ff,
																cid & 0x000000ff);

				if (rcp==NULL) {
								(*prcp)=rc;
								rc->prior=NULL;
								rc->next=NULL;
				} else {
								rcp->next=rc;
								rc->prior=rcp;
				}

				return rc;
}

void help() {
	log_info ("fixwav - GSP 0.1");
	log_info (" -i <input file> ");
	log_info (" -o <output file> ");
	log_info (" -h ");

}
int main(int argc, char *argv[]) {
				log_info("fixwav 0.1");
				int c,index;
				int aflag=0;
				int bflag=0;
				char *ivalue=NULL;
				char *ovalue=NULL;

				while ((c = getopt(argc,argv,"i:o:h")) != -1)
					switch (c) {
						case 'i':
							ivalue=optarg;
						break;
						case 'o':
							ovalue=optarg;
							doFix=1;
						break;
						case'h':
							help();
							exit(0);
						break;
						case '?':
							if (optopt=='i')
								log_error("option 'i' needs an argument");
							else if (optopt=='o')
								log_error("option 'o' needs an argument");
							else
								log_error("unknown option");
						break;
						default:
								log_error("dropthrough error");
							exit(1);
					}
						
				if (!ivalue) {
					log_error("need an input file at least to continue");
					exit(1);
				}

				struct riff_chunk * arc;

				struct riff_chunk * rc1=addChunk(&chunklist,'RIFF');
				addChunk(&rc1->subchunks,'INFO');
				addChunk(&rc1->subchunks,'JUNK');
				addChunk(&rc1->subchunks,'bext');
				addChunk(&rc1->subchunks,'iXML');
				addChunk(&rc1->subchunks,'fmt ');
				addChunk(&rc1->subchunks,'data');

				unsigned char * buf=calloc(sizeof(char),BLOCK_SIZE);
				FILE * f_in=fopen(ivalue,"r");

				if (!f_in) {
					log_error ("unable to open file %s",ivalue);
					exit(1);
				}

				fseek(f_in,0L,SEEK_END);
				uint64_t actualFileSize=(uint64_t) ftell(f_in);
				fseek(f_in,0L,SEEK_SET);


				log_info("file %s open, size=%u",ivalue,actualFileSize);
				if (!ovalue) {
					log_info("No Output File supplied - Dry Run Scan only.");
				}

				int64_t cnt=0;
				int64_t ptr=0; //working position within block
				int64_t tsize=0; //total size of data read
				int64_t position=0; //position within file so far
				struct riff_chunk * acPtr; //active chunk
				//int blk=0;
				int chunkOrder=0;
				double pct=0;
				int pctCount=0;

				while (1) {

								cnt=fread((void*)buf,sizeof(char),BLOCK_SIZE,f_in);

								if (!cnt) break;

								if (cnt==BLOCK_SIZE) {
												fseek(f_in,-(BLOCK_SIZE/2),SEEK_CUR);
												cnt=(BLOCK_SIZE/2);
								}

								tsize+=cnt; // total size read so far

								pct=100.0*((double)tsize/(double)actualFileSize);

								if (!(++pctCount % 200))
									log_info("scanning..... %.1f complete",pct);

								ptr=0; //where are we in this block

								while (cnt--) {

												if (acPtr=detectChunkBoundary(chunklist,(FOURCC *)(((void*)buf)+ptr))) {
																if (0)
																				log_info("chunk boundary %c%c%c%c",
																												(acPtr->chunk_id >> 0) & 0x000000ff,
																												(acPtr->chunk_id >> 8) & 0x000000ff,
																												(acPtr->chunk_id >> 16) & 0x000000ff,
																												(acPtr->chunk_id >> 24 )& 0x000000ff);
																acPtr->start_offset=position;
																acPtr->reported_length=(DWORD)(*((DWORD*)(buf+ptr+sizeof(FOURCC))));
																acPtr->actual_length=0; 
																acPtr->present=1; // we have seen this chunk in the file
																acPtr->chunkOrder=chunkOrder++;
																cnt-=8; ptr+=8; //bypass the id and data segment
																position+=8;
																struct riff_chunk * rc=getFirstChunk(acPtr); //get the root chunk for this branch
																iterateChunks(rc,clearActive); // clear active status on all chunks in this chain
																iterateChunks(chunklist,incrementDataLength8);
																acPtr->active=1;
												}


												iterateChunks(chunklist,incrementDataLength);

												ptr++;
												position++;

								}

				}

				iterateChunks(chunklist,checkChunk);
				iterateChunks(chunklist,printChunk);

				log_info("total size=%#08X, %li",tsize,tsize);

				if (doFix) {
								fseek(f_in,0L,SEEK_SET);
								FILE * f_out=fopen(ovalue,"w");
								if (!f_out) {
									log_error("unable to open output file %s",ovalue);
									fclose(f_in);
									exit(1);
								}
								log_info("Fixing File into %s",ovalue);
								transferChunks(chunklist,chunkReadWrite,f_in,f_out);
								fclose(f_out);
				}
				log_info("Complete.");

				fclose(f_in);
				exit(0);

}

