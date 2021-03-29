/************************************************
Title: fake_gas.c
Author: Jared Coughlin
Date: 5/4/17
Purpose: Temporarily changes all dm particles to type 0
         so gadget's tree in dspec will get the densities
         and smoothing lengths for me. dspec changes them back.
Notes:
************************************************/
#include <stdlib.h>
#include <stdio.h>
#include <string.h>



/***********************
      Globals
***********************/
typedef struct IO_HEADER
{
	int npart[6];
	double mass[6];
	double time;
	double redshift;
   int flag_sfr;
	int flag_feedback;
	int npartTotal[6];
	int flag_cooling;
	int num_files;
	double BoxSize;
	double Omega0;
	double OmegaLambda;
	double HubbleParam;
	char fill[96];
} IO_HEADER;

typedef struct PARTICLE_DATA
{
	float Pos[3];
	float Vel[3];
	int Type, id;
	float Mass, U;
} PARTICLE_DATA;



// Fields for block checking
enum fields
{
   HEADER,
   POS,
   VEL,
   IDS,
   MASS,
   U,
};

// Global header. I think this is safe to do
IO_HEADER m_header;



/***********************
      Prototypes
***********************/
PARTICLE_DATA *read_snapshot(char *);
size_t my_fread(void *, size_t, size_t, FILE *);
size_t my_fwrite(void *, size_t, size_t, FILE *);
void block_check(enum fields, int, int, IO_HEADER);
int get_block_size(enum fields, IO_HEADER);
void fake_gas(char *, PARTICLE_DATA *);



/***********************
         main
***********************/
int main(int argc, char **argv)
{
   int nfiles;
   int i;
   char snap_filename[256];
   PARTICLE_DATA *M_P;

   // Check args
   if(argc < 2)
   {
      printf("Error, incorrect number of args! Just need snapshots to,\n");
      printf("convert, e.g. snapshot_001 snapshot_002\n");
      exit(EXIT_FAILURE);
   }

   // Get the number of files passed
   nfiles = argc - 1;

   // Loop over and convert each file
   for(i = 1; i <= nfiles; i++)
   {
      // Read in the args
      sprintf(snap_filename, "%s", argv[i]);

      // Read the snapshot
      M_P = read_snapshot(snap_filename);

      // Remove gas particles
      fake_gas(argv[i], M_P);

      // Free memory and reset snapshot name
      free(M_P);
   }

   return 0;
}



/***********************
      read_snapshot
***********************/
PARTICLE_DATA *read_snapshot(char *snapname)
{
	// Read the current snapshot

	FILE *fd;
   int i;
	int n;
   int k; 
   int pc_new;
   int blksize1;
   int blksize2;
   int HEADER_SIZE = 256;
   int NumPart = 0;
   int nwithmass = 0;
   enum fields blocknr;
   PARTICLE_DATA *P = NULL;

   // Open file
   if(!(fd = fopen(snapname, "rb")))
   {
      printf("Error, could not read file!\n");
      exit(EXIT_FAILURE);
   }

   // Read the header from the file
   my_fread(&blksize1, sizeof(int), 1, fd);
   my_fread(&m_header, HEADER_SIZE, 1, fd);
   my_fread(&blksize2, sizeof(int), 1, fd);

   blocknr = HEADER;
   block_check(blocknr, blksize1, blksize2, m_header);

   // Get number of particles in file and nwithmasses
   for(i = 0; i < 6; i++)
   {
      NumPart += m_header.npart[i];

      if(m_header.mass[i] == 0)
      {
         nwithmass += m_header.npart[i];
      }
   }
   

	if(!(P = malloc(NumPart * sizeof(PARTICLE_DATA))))
	{
		printf("Could not allocate memory for particles.\n");
		exit(EXIT_FAILURE);
	}	

	// Positions
   my_fread(&blksize1, sizeof(int), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fread(&P[pc_new].Pos[0], sizeof(float), 3, fd);
			pc_new++;
		}
	}
   my_fread(&blksize2, sizeof(int), 1, fd);

   blocknr = POS;
   block_check(blocknr, blksize1, blksize2, m_header);

	// Velocities
   my_fread(&blksize1, sizeof(int), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fread(&P[pc_new].Vel[0], sizeof(float), 3, fd);
			pc_new++;
		}
	}
   my_fread(&blksize2, sizeof(int), 1, fd);

   blocknr = VEL;
   block_check(blocknr, blksize1, blksize2, m_header);

	// Ids
   my_fread(&blksize1, sizeof(int), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fread(&P[pc_new].id, sizeof(int), 1, fd);														
			pc_new++;
		}
	}
   my_fread(&blksize2, sizeof(int), 1, fd);

   blocknr = IDS;
   block_check(blocknr, blksize1, blksize2, m_header);

	// Masses
	if(nwithmass > 0)
	{
      my_fread(&blksize1, sizeof(int), 1, fd);
	}
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			P[pc_new].Type = k;
			if(m_header.mass[k] == 0)
			{
				my_fread(&P[pc_new].Mass,sizeof(float),1, fd);
			}
			else
			{
				P[pc_new].Mass = m_header.mass[k];
			}
			pc_new++;
		}
	}
	if(nwithmass > 0)
	{
      my_fread(&blksize2, sizeof(int), 1, fd);

      blocknr = MASS;
      block_check(blocknr, blksize1, blksize2, m_header);
	}
	
	// Clean up
	fclose(fd);

	return P;
}



/***********************
        my_fread
***********************/
size_t my_fread(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   /* This function is taken from gadget. It checks to make sure fread has 
      read the correct number of elements. It handles the compiler warning about 
      ignoring the return value of the function when using -O3. */

   size_t nread;

   if((nread = fread(ptr, size, nmemb, stream)) != nmemb)
   {
      printf("I/O error with fread.\n");
      exit(EXIT_FAILURE);
   }

   return nread;
}



/***********************
        my_fwrite
***********************/
size_t my_fwrite(void *ptr, size_t size, size_t nmemb, FILE *stream)
{
   /* This function is taken from gadget. It checks to make sure fwrite has 
      written the correct number of elements. It handles the compiler warning about 
      ignoring the return value of the function when using -O3. */

   size_t nwritten;

   if((nwritten = fwrite(ptr, size, nmemb, stream)) != nmemb)
   {
      printf("I/O error with fwrite.\n");
      exit(EXIT_FAILURE);
   }

   return nwritten;
}



/***********************
      block_check
***********************/
void block_check(enum fields blocknr, int blksize1, int blksize2, IO_HEADER dummy_header)
{
   // This function gets the size of block and makes sure that
   // the padding values that are written around it (the ones 
   // obtained with SKIP) match that value.

   int size;

   // Make sure the two padding values match
   if(blksize1 != blksize2)
   {
      printf("Paddings don't match! Block: %d\n", blocknr);
      exit(EXIT_FAILURE);
   }

   // Make sure the padding matches the actual size of the block
   size = get_block_size(blocknr, dummy_header);

   if(blksize1 != size)
   {
      printf("Paddings do not match actual block size! Block: %d\n", blocknr);
      exit(EXIT_FAILURE);
   }
}



/***********************
    get_block_size
***********************/
int get_block_size(enum fields blocknr, IO_HEADER h)
{
   // This function gets the actual block size to make sure that the correct
   // values are written to the padding.

   int bsize;
   int nwithmass = 0;
   int i;

   for(i = 0; i < 6; i++)
   {
      if(h.mass[i] == 0)
      {
         nwithmass += h.npart[i];
      }
   }

   switch(blocknr)
   {
      case HEADER:
         bsize = sizeof(IO_HEADER);
         break;

      case POS:
      case VEL:
         bsize = sizeof(float) * 3 * (h.npart[0] + h.npart[1] + h.npart[2] + 
                 h.npart[3] + h.npart[4] + h.npart[5]); 
         break;

      case IDS:
         bsize = sizeof(int) * (h.npart[0] + h.npart[1] + h.npart[2] + h.npart[3] +
                 h.npart[4] + h.npart[5]);
         break;

      case MASS:
         bsize = sizeof(float) * nwithmass;
         break;

      case U:
         bsize = sizeof(float) * h.npart[0];
         break;
   }

   return bsize;
}



/***********************
        fake_gas
***********************/
void fake_gas(char *filename, PARTICLE_DATA *M_P)
{
   // Changes dm particles to type 0 so gadget's tree in
   // dspec will get hsml and rho for those particles for me.
   // Gadget also expects the U block to be written if there are
   // gas particles, so I have to add that block here, as well. Just
   // fill it with zeros

	FILE *fd;
	int n; 
   int k;
   int blksize;
   int pc_new;
   int nwithmass = 0;
   int i;
   char newname[256];
   //char zname[256];
   char *split_str;


   // Set up the new file. The name is snapshot-fakegas_xxx.y

   // Split the string on an underscore. We can give more than one
   // delimiter here in the "" and it will look for any of them.
   // NOTE: splitting on an underscore is wrong if there are underscores in the path
   split_str = strtok(filename, "_");

   // Now we copy the first segment of the string to master_str
   strcpy(newname, split_str);

   // Now we append -fakegas_ to the master string
   strcat(newname, "-fakegas_");

   // Now we call strtok again. It requires a NULL pointer on
   // calls after the first and it moves to the next segment
   // We include the . because I need the snapshot number
   // in order to write the redshift for use in make_params

   // There's only a dot if there's more than one file per snapshot
   if(m_header.num_files > 1)
   {
      split_str = strtok(NULL, "_.");
   }

   else
   {
      split_str = strtok(NULL, "_");
   }

   // Make redshift file
   //sprintf(zname, "./params/redshift_snapshot_%s.txt", split_str);
   //if(!(fd = fopen(zname, "w")))
   //{
   //   printf("Error, could not open file for writing redshift!\n");
   //   exit(EXIT_FAILURE);
   //}

   //fprintf(fd, "%lf", m_header.redshift);

   //fclose(fd);

   // Now we append the second segment to the name
   strcat(newname, split_str);

   // Have to call strtok one last time to get the file number for the snapshot
   // if there's more than one file per snapshot
   if(m_header.num_files > 1)
   {
      split_str = strtok(NULL, "_");

      // Append the number
      strcat(newname, ".");
      strcat(newname, split_str);
   }

	// Open file
	if(!(fd = fopen(newname, "wb")))
	{
		printf("Can't open '%s' for writing snapshot.\n", newname);
		exit(EXIT_FAILURE);
	}

   // Change number of gas and dm particles
   m_header.npart[0] = m_header.npart[1];
   m_header.npartTotal[0] = m_header.npartTotal[1];
   m_header.npart[1] = 0;
   m_header.npartTotal[1] = 0;
   m_header.mass[0] = m_header.mass[1];
   m_header.mass[1] = 0.0;

	/*Write header*/
   blksize = sizeof(m_header);

	my_fwrite(&blksize, sizeof(blksize), 1, fd);
	my_fwrite(&m_header, sizeof(m_header), 1, fd);
	my_fwrite(&blksize, sizeof(blksize), 1, fd);

	/*Positions*/
   blksize = (m_header.npart[0] + m_header.npart[1] + m_header.npart[2] + 
              m_header.npart[3] + m_header.npart[4] + 
              m_header.npart[5]) * sizeof(float) * 3.0;
	my_fwrite(&blksize, sizeof(blksize), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fwrite(&M_P[pc_new].Pos[0], sizeof(float), 3, fd);								
			pc_new++;
		}
	}
	my_fwrite(&blksize, sizeof(blksize), 1, fd);

	/*Velocities*/
	my_fwrite(&blksize, sizeof(blksize), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fwrite(&M_P[pc_new].Vel[0], sizeof(float), 3, fd);
			pc_new++;
		}
	}
	my_fwrite(&blksize, sizeof(blksize), 1, fd);

	/*Ids*/
   blksize = (m_header.npart[0] + m_header.npart[1] + m_header.npart[2] + 
              m_header.npart[3] + m_header.npart[4] + 
              m_header.npart[5]) * sizeof(int);
	my_fwrite(&blksize, sizeof(blksize), 1, fd);
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			my_fwrite(&M_P[pc_new].id, sizeof(int), 1, fd);														
			pc_new++;
		}
	}
	my_fwrite(&blksize, sizeof(blksize), 1, fd);

	/*Masses*/
   for(i = 0; i < 6; i++)
   {
      if(m_header.mass[i] == 0)
      {
         nwithmass += m_header.npart[i];
      }
   }

   blksize = nwithmass * sizeof(float);

	if(nwithmass > 0)
	{
	   my_fwrite(&blksize, sizeof(blksize), 1, fd);
	}
	for(k = 0, pc_new = 0; k < 6; k++)
	{
		for(n = 0; n < m_header.npart[k]; n++)
		{
			if(m_header.mass[k] == 0)
			{
				my_fwrite(&M_P[pc_new].Mass,sizeof(float),1, fd);
			}
			pc_new++;
		}
	}
	if(nwithmass > 0)
	{
	   my_fwrite(&blksize, sizeof(blksize), 1, fd);
	}

   // U
   float U = 0.0;
   blksize = m_header.npart[0] * sizeof(float);
	my_fwrite(&blksize, sizeof(blksize), 1, fd);
   for(n = 0; n < m_header.npart[0]; n++)
   {
      my_fwrite(&U, sizeof(float), 1, fd);
   }
	my_fwrite(&blksize, sizeof(blksize), 1, fd);
	
	/*Clean up*/
	fclose(fd);
}
