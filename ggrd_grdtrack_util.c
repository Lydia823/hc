/* 
   
subroutines for 3-D interpolation of scalar data in GMT grd files
based on GMT3.4.3 grdtrack


$Id: ggrd_grdtrack_util.c,v 1.6 2006/02/16 02:18:03 twb Exp twb $


original comments for grdtrack from GMT at bottom of file
 
*/

#include "gmt.h"
#include "ggrd_grdtrack_util.h"
#ifndef ONEEIGHTYOVERPI
#define ONEEIGHTYOVERPI  57.295779513082320876798154814105
#endif
#include <math.h>

/* 

wrapper

*/



/* 
   init structure and files for the general case, this is a wrapper
   for what's below
   
   is_three: TRUE/FALSE for 3-D/2-D
   grdfile: filename for 2-D and prefix for 3-D
   depth_file: file with depth layers for 3-D
   edge_info_string: as in GMT 
   g: ggrd_gt structure 

   
   returns error code

*/
int ggrd_grdtrack_init_general(unsigned char is_three,char *grdfile, char *depth_file,
			       char *gmt_edgeinfo_string,
			       struct ggrd_gt *g,unsigned char verbose,
			       unsigned char change_z_sign)
{
  static unsigned char bilinear = FALSE; /* cubic by default */
  static double west=0.,east=0.,south=0.,north=0.; /* whole region by default */
  int pad[4];
  unsigned char geographic_in = TRUE;
  int i;
  float zavg;
  if(g->init){
    fprintf(stderr,"ggrd_grdtrack_init_general: error: call only once\n");
    return 1;
  }
  g->is_three = is_three;
#ifdef USE_GMT4
  if(ggrd_grdtrack_init(&west,&east,&south,&north,&g->f,&g->mm,
			grdfile,&g->grd,&g->edgeinfo,
			gmt_edgeinfo_string,&geographic_in,
			pad,is_three,depth_file,&g->z,&g->nz,
			bilinear,verbose,change_z_sign,&g->bcr))
    return 2;
#else
  if(ggrd_grdtrack_init(&west,&east,&south,&north,&g->f,&g->mm,
			grdfile,&g->grd,&g->edgeinfo,
			gmt_edgeinfo_string,&geographic_in,
			pad,is_three,depth_file,&g->z,&g->nz,
			bilinear,verbose,change_z_sign))
    return 2;
#endif
  if(is_three){
    /* 
       check how the depth levels are specified for debugging 
    */
    zavg = 0.0;
    for(i=0;i < g->nz;i++)
      zavg += g->z[i];
    if(zavg > 0)
      g->zlevels_are_negative = FALSE;
    else
      g->zlevels_are_negative = TRUE;
  }else{
    g->zlevels_are_negative = FALSE;
  }
  //  if(change_z_sign)		/* reverse logic */
  //g->zlevels_are_negative = (g->zlevels_are_negative)?(FALSE):(TRUE);
  if(verbose){
    fprintf(stderr,"ggrd_grdtrack_init_general: initialized from %s, %s.\n",
	    grdfile,(is_three)?("3-D"):("1-D"));
    if(is_three){
      fprintf(stderr,"ggrd_grdtrack_init_general: depth file %s, %i levels, %s.\n",
	      depth_file,g->nz,
	      (g->zlevels_are_negative)?("negative z levels"):("positive z levels"));
    }
  }
  g->init = TRUE;
  return 0;
}


/* 

take log10, 10^x and/or scale complete grid. log10 and 10^x applies first

 */
int ggrd_grdtrack_rescale(struct ggrd_gt *g,
			  unsigned char take_log10, /* take log10() */
			  unsigned char take_power10, /* take 10^() */
			  unsigned char rescale, /* rescale? */
			  double scale	/* factor for rescaling */)
{
  int i,j,k;
  if(!g->init){
    fprintf(stderr,"ggrd_grdtrack_rescale: error: ggrd not initialized\n");
    return 1;
  }
  for(i=0;i < g->nz;i++){	/* loop through depths */
    k = i*g->mm;
    for(j=0;j < g->mm;j++,k++){
      if(take_log10){
	g->f[k] = log10(g->f[k]);
      }
      if(take_power10){
	g->f[k] = pow(10.0,g->f[k]);
      }
      if(rescale)
	g->f[k] *= scale;
    }
  }
  return 0;
}


/* 
   interpolation wrapper, uses r, theta, phi input. return value and TRUE if success,
   undefined and FALSE else
*/
unsigned char ggrd_grdtrack_interpolate_rtp(double r,double t,double p,
					    struct ggrd_gt *g,double *value,
					    unsigned char verbose)
{
  double x[3];
  unsigned char result;
  if(!g->init){			/* this will not necessarily work */
    fprintf(stderr,"ggrd_grdtrack_interpolate_rtp: error, g structure not initialized\n");
    exit(-1);
  }
  if(!g->is_three){
    fprintf(stderr,"ggrd_grdtrack_interpolate_rtp: error, g structure is not 3-D\n");
    exit(-1);
  }
  /* 
     convert coordinates to lon / lat / z
  */
  x[0] = p * ONEEIGHTYOVERPI; /* lon */
  /* make sure we are in 0 ... 360 system */
  if(x[0]<0)
    x[0]+=360.0;
  if(x[0]>=360)
    x[0]-=360.0;
  x[1] = 90.0 - t * ONEEIGHTYOVERPI; /* lat */
  x[2] = (1.0-r) * 6371.0;	/* depth in [km] */
  if(g->zlevels_are_negative)	/* adjust for depth */
    x[2] = -x[2];
#ifdef USE_GMT4
  result = ggrd_grdtrack_interpolate(x,TRUE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose,&g->bcr);
#else
  result = ggrd_grdtrack_interpolate(x,TRUE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose);
#endif
  return result;
}
/* 
   interpolation wrapper, uses x, y, z input. return value and TRUE if success,
   undefined and FALSE else
*/
unsigned char ggrd_grdtrack_interpolate_xyz(double x,double y,double z,
					    struct ggrd_gt *g,double *value,
					    unsigned char verbose)
{
  double xloc[3];
  unsigned char result;
  if(!g->init){			/* this will not necessarily work */
    fprintf(stderr,"ggrd_grdtrack_interpolate_xyz: error, g structure not initialized\n");
    exit(-1);
  }
  if(!g->is_three){
    fprintf(stderr,"ggrd_grdtrack_interpolate_xyz: error, g structure is not 3-D\n");
    exit(-1);
  }
  /* 
     convert coordinates
  */
  xloc[0] = x; /* lon, x */
  xloc[1] = y; /* lat, y */
  xloc[2] = z;	/* depth, z*/
  if(g->zlevels_are_negative)	/* adjust for depth */
    xloc[2] = -xloc[2];
#ifdef USE_GMT4
  result = ggrd_grdtrack_interpolate(xloc,TRUE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose,&g->bcr);
#else
  result = ggrd_grdtrack_interpolate(xloc,TRUE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose);

#endif
  return result;
}

/* 
   interpolation wrapper, uses theta, phi input. return value and TRUE if success,
   undefined and FALSE else
*/
unsigned char ggrd_grdtrack_interpolate_tp(double t,double p,struct ggrd_gt *g,double *value,
					   unsigned char verbose)
{
  double x[3];
  unsigned char result;
  if(!g->init){			/* this will not necessarily work */
    fprintf(stderr,"ggrd_grdtrack_interpolate_tp: error, g structure not initialized\n");
    exit(-1);
  }
  if(g->is_three){
    fprintf(stderr,"ggrd_grdtrack_interpolate_tp: error, g structure is not 2-D\n");
    exit(-1);
  }
  /* 
     convert coordinates
  */
  x[0] = p * ONEEIGHTYOVERPI; /* lon */
  if(x[0]<0)
    x[0]+=360.0;
  if(x[0]>=360)
    x[0]-=360.0;
  x[1] = 90.0 - t * ONEEIGHTYOVERPI; /* lat */
  x[2] = 1.0;
#ifdef USE_GMT4
  result = ggrd_grdtrack_interpolate(x,FALSE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose,&g->bcr);
#else
  result = ggrd_grdtrack_interpolate(x,FALSE,g->grd,g->f,g->edgeinfo,g->mm,g->z,g->nz,
				     value,verbose);
#endif
  return result;
}
/* 

free structure

*/
void ggrd_grdtrack_free_gstruc(struct ggrd_gt *g)
{
  free(g->grd);
  free(g->edgeinfo);
  free(g->f);
  if(g->is_three)
    free(g->z);
}
/* 

given a location vector in spherical theta, phi system (xp[3]) and a
Cartesian rotation vector wx, wy, wz (omega[3]), find the spherical
velocities vr, vtheta,vphi

*/
void ggrd_find_spherical_vel_from_rigid_cart_rot(double *vr,
						 double *vtheta,
						 double *vphi,
						 double *xp,
						 double *omega)
{
  double vp[3],polar_base[3][3],ct,cp,st,sp,xc[3],tmp,vc[3];
  int i;
  /* cos and sin theta and phi */
  ct=cos(xp[1]);cp=cos(xp[2]);
  st=sin(xp[1]);sp=sin(xp[2]);
  /* convert location to Cartesian */
  tmp =  st * xp[0];
  xc[0]= tmp * cos(xp[2]);	/* x */
  xc[1]= tmp * sin(xp[2]);	/* y */
  xc[2]= ct * xp[0];		/* z */
  /* v = omega \cross r */
  vc[0] = omega[1]*xc[2] - omega[2]*xc[1];
  vc[1] = omega[2]*xc[0] - omega[0]*xc[2];
  vc[2] = omega[0]*xc[1] - omega[1]*xc[0];
  /* get basis */
  polar_base[0][0]= st * cp;polar_base[0][1]= st * sp;polar_base[0][2]= ct;
  polar_base[1][0]= ct * cp;polar_base[1][1]= ct * sp;polar_base[1][2]= -st;
  polar_base[2][0]= -sp;polar_base[2][1]= cp;polar_base[2][2]= 0.0;
  /* convert */
  for(i=0;i<3;i++){
    vp[i]  = polar_base[i][0] * vc[0];
    vp[i] += polar_base[i][1] * vc[1];
    vp[i] += polar_base[i][2] * vc[2];
  }
  vr[0] = vp[0];
  vtheta[0] = vp[1];
  vphi[0]=vp[2];
  


}
						 
/* 

initialize

*/
#ifdef USE_GMT4
int ggrd_grdtrack_init(double *west, double *east,double *south, double *north, 
			/* geographic bounds,
			   set all to zero to 
			   get the whole range from the
			   input grid files
			*/
		       float **f,	/* data, pass as empty */
		       int *mm,  /* size of data */
		       char *grdfile,	/* name, or prefix, of grd file with scalars */
		       struct GRD_HEADER **grd,	/* pass as empty */
		       struct GMT_EDGEINFO **edgeinfo, /* pass as empty */
		       char *edgeinfo_string, /* -L type flags from GMT, can be empty */
		       unsigned char *geographic_in, /* this is normally TRUE */
		       int *pad,	/* [4] array with padding (output) */
		       unsigned char three_d, char *dfile, 	/* depth file name */
		       float **z,	/* layers, pass as NULL */
		       int *nz,		/* number of layers */
		       unsigned char bilinear, /* linear/cubic? */
		       unsigned char verbose,
		       unsigned char change_depth_sign, /* change the
							  sign of the
							  depth
							  levels to go from depth (>0) to z (<0) */
		       struct GMT_BCR *bcr)
#else
int ggrd_grdtrack_init(double *west, double *east,double *south, double *north, 
		       float **f,int *mm,char *grdfile,struct GRD_HEADER **grd,
		       struct GMT_EDGEINFO **edgeinfo,char *edgeinfo_string, 
		       unsigned char *geographic_in,int *pad,unsigned char three_d, 
		       char *dfile, 	float **z,int *nz,		
		       unsigned char bilinear,unsigned char verbose,
		       unsigned char change_depth_sign)
#endif
{
  FILE *din;
  float dz1,dz2;
  struct GRD_HEADER ogrd;
  int i,one_or_zero,nx,ny,mx,my,nn;
  char filename[BUFSIZ*2];
  /* 
     deal with edgeinfo 
  */
  *edgeinfo = (struct GMT_EDGEINFO *)
    GMT_memory (VNULL, (size_t)1, sizeof(struct GMT_EDGEINFO), "ggrd_grdtrack_init");
  /* 
     init first edgeinfo 
  */
  GMT_boundcond_init (*edgeinfo);
  if (edgeinfo_string){
    GMT_boundcond_parse (*edgeinfo, edgeinfo_string);
    if ((*edgeinfo)[0].gn) 
      *geographic_in = TRUE;
  }
  
  *z = (float *) GMT_memory (VNULL, (size_t)1, sizeof(float), "ggrd_grdtrack_init");
 
  if(three_d){
    /*
      
    three D part first
    
    */
    /* 
       init the layers
    */
    din = fopen(dfile,"r");
    if(!din){
      fprintf(stderr,"ggrd_grdtrack_init: could not open depth file %s\n",
	      dfile);
      return 1;
    }
    /* read in the layers */
    *nz = 0;
    dz1 = -1;
    while(fscanf(din,"%f",(*z+ (*nz))) == 1){ 
      if(change_depth_sign)
	*(*z+ (*nz)) = -(*(*z+ (*nz)));
      /* read in each depth layer */
      *z = (float *) GMT_memory ((void *)(*z), (size_t)((*nz)+2), sizeof(float), "ggrd_grdtrack_init");
      if(*nz > 0){		/* check for increasing layers */
	if(dz1 < 0){	
	  /* init first interval */
	  dz1 = *(*z+(*nz)) - *(*z+(*nz)-1);
	  dz2 = dz1;
	}else{
	  /* later intervals */
	  dz2 = *(*z+(*nz)) - *(*z+(*nz)-1);
	}
	if(dz2 <= 0.0){		/* check for monotonic increase */
	  fprintf(stderr,"%s: error: levels in %s have to increase monotonically: n: %i dz; %g\n",
		  "ggrd_grdtrack_init",dfile,*nz,dz2);
	  return 2;
	}
      }
      *nz += 1;
    }
    fclose(din);
    /* end layer init"ggrd_grdtrack_initialization */
    if(*nz < 2){
      fprintf(stderr,"%s: error: need at least two layers in %s\n",
	      "ggrd_grdtrack_init", dfile);
      return 3;
    }
    if(verbose)
      fprintf(stderr,"%s: read %i levels from %s between zmin: %g and zmax: %g\n",
	      "ggrd_grdtrack_init",*nz,dfile,*(*z+0),*(*z+(*nz)-1));
  }else{
    *nz = 1;
    *(*z) = 0.0;
    if(verbose >= 2)
      fprintf(stderr,"ggrd_grdtrack_init: single level at z: %g\n",*(*z));
  }
  /* 
     get nz grd and edgeinfo structures 
  */
  *grd = (struct GRD_HEADER *)
    GMT_memory (NULL, (size_t)(*nz), sizeof(struct GRD_HEADER), "ggrd_grdtrack_init");
  *edgeinfo = (struct GMT_EDGEINFO *)
    GMT_memory (*edgeinfo, (size_t)(*nz), sizeof(struct GMT_EDGEINFO), "ggrd_grdtrack_init");
  if(verbose >= 2)
    fprintf(stderr,"ggrd_grdtrack_init: mem alloc ok\n");
  if(*nz == 1){
    if(verbose >= 2)
      fprintf(stderr,"ggrd_grdtrack_init: opening single file %s\n",grdfile);
    if (GMT_cdf_read_grd_info (grdfile,(*grd))) {
      fprintf (stderr, "%s: error opening file %s\n", 
	       "ggrd_grdtrack_init", grdfile);
      return 4;
    }
  }else{
    /* loop through headers for testing purposess */
    for(i=0;i<(*nz);i++){
      sprintf(filename,"%s.%i.grd",grdfile,i+1);
      if (GMT_cdf_read_grd_info (filename, (*grd+i))) {
	fprintf (stderr, "%s: error opening file %s (-D option was used)\n", 
		 "ggrd_grdtrack_init", filename);
	return 6;
      }
      if(i == 0){
	/* save the first grid parameters */
	ogrd.x_min = (*grd)[0].x_min;
	ogrd.y_min = (*grd)[0].y_min;
	ogrd.x_max = (*grd)[0].x_max;
	ogrd.y_max = (*grd)[0].y_max;
	ogrd.x_inc = (*grd)[0].x_inc;
	ogrd.y_inc = (*grd)[0].y_inc;
	ogrd.node_offset = (*grd)[0].node_offset;
	ogrd.nx = (*grd)[0].nx;
	ogrd.ny = (*grd)[0].ny;
	/* 
	   
	make sure we are in 0 ... 360 system

	*/
	if((ogrd.x_min < 0)||(ogrd.x_max<0)){
	  fprintf(stderr,"%s: WARNING: geographic grids should be in 0..360 lon system (found %g - %g)\n",
		  "ggrd_grdtrack_init",ogrd.x_min,ogrd.x_max);
	}
      }else{
	/* test */
	if((fabs(ogrd.x_min -  (*grd)[i].x_min)>5e-7)||
	   (fabs(ogrd.y_min -  (*grd)[i].y_min)>5e-7)||
	   (fabs(ogrd.x_max -  (*grd)[i].x_max)>5e-7)||
	   (fabs(ogrd.y_max -  (*grd)[i].y_max)>5e-7)||
	   (fabs(ogrd.x_inc -  (*grd)[i].x_inc)>5e-7)||
	   (fabs(ogrd.y_inc -  (*grd)[i].y_inc)>5e-7)||
	   (fabs(ogrd.nx    -  (*grd)[i].nx)>5e-7)||
	   (fabs(ogrd.ny    -  (*grd)[i].ny)>5e-7)||
	   (fabs(ogrd.node_offset - (*grd)[i].node_offset)>5e-7)){
	  fprintf(stderr,"%s: error: grid %i out of %i has different dimensions or setting from first\n",
		 "ggrd_grdtrack_init",i+1,(*nz));
	  return 8;
	}
      }
    }
  }
  if(verbose > 2)
    fprintf(stderr,"ggrd_grdtrack_init: read %i headers OK, grids appear to be same size\n",*nz);
  if (fabs(*west - (*east)) < 5e-7) {	/* No subset asked for , west same as east*/
    *west = (*grd)[0].x_min;
    *east = (*grd)[0].x_max;
    *south = (*grd)[0].y_min;
    *north = (*grd)[0].y_max;
  }
  one_or_zero = ((*grd)[0].node_offset) ? 0 : 1;
  nx = irint ( (*east - *west) / (*grd)[0].x_inc) + one_or_zero;
  ny = irint ( (*north - *south) / (*grd)[0].y_inc) + one_or_zero;
  /* real size of data */
  nn = nx * ny;

  /* padded */
  mx = nx + 4;
  my = ny + 4;
  /* 
     get space for all layers
  */
  *mm = mx * my;

  *f = (float *) malloc((size_t)((*mm) * (*nz) * sizeof (float)));
  if(!(*f)){
    fprintf(stderr,"ggrd_grdtrack_init: f memory error\n");
    return 9;
  }
  if(verbose >= 2)
    fprintf(stderr,"ggrd_grdtrack_init: mem alloc 2 ok\n");

  /* 
     pad on sides 
  */
  pad[0] = pad[1] = pad[2] = pad[3] = 2;
  if (verbose) 
    fprintf(stderr,"ggrd_grdtrack_init: reading grd files:\n");
  for(i=0;i < (*nz);i++){
    /* 
       loop through layers
    */
    if(i != 0)			/* copy first edgeinfo over */
      memcpy((*edgeinfo+i),(*edgeinfo),sizeof(struct GMT_EDGEINFO));
    if((*nz) == 1){
      sprintf(filename,"%s",grdfile);
    }else{			/* construct full filename */
      sprintf(filename,"%s.%i.grd",grdfile,i+1);
    }
    /* 
       read the grd files
    */
#ifdef USE_GMT4
    if (GMT_cdf_read_grd (filename, (*grd+i), (*f+i* (*mm)), 
			  *west, *east, *south, *north, 
			  pad, FALSE,NC_FLOAT)) {
      fprintf (stderr, "%s: error reading file %s\n", "ggrd_grdtrack_init", grdfile);
      return 10;
    }
#else
    if (GMT_cdf_read_grd (filename, (*grd+i), (*f+i* (*mm)), 
			  *west, *east, *south, *north, 
			  pad, FALSE)) {
      fprintf (stderr, "%s: error reading file %s\n", "ggrd_grdtrack_init", grdfile);
      return 10;
    }
#endif
    /* prepare the boundaries */
    GMT_boundcond_param_prep ((*grd+i), (*edgeinfo+i));
    if(i == 0){
      
      /* 
	 Initialize bcr structure, this can be the same for 
	 all grids as long as they have the same dimesnions

      */
#ifdef USE_GMT4
      GMT_bcr_init ((*grd+i), pad, bilinear,1.0,bcr);
#else
      GMT_bcr_init ((*grd+i), pad, bilinear);
#endif
     }
    /* Set boundary conditions  */
    GMT_boundcond_set ((*grd+i), (*edgeinfo+i), pad, 
		       (*f+i*(*mm)));
     
  } /* end layer loop */
  if(verbose)
    ggrd_print_layer_avg(*f,*z,*mm,*nz,stderr);
  return 0;

}
void ggrd_print_layer_avg(float *x,float *z,int n, int m,FILE *out)
{
  int i;
  for(i=0;i<m;i++)
    fprintf(stderr,"ggrd_grdtrack_init: layer %3i at depth %11g, mean: %11g rms: %11g\n",
	    i+1,z[i],ggrd_gt_mean((x+i*n),n),ggrd_gt_rms((x+i*n),n));
}


/* 

interpolate value 

 */
#ifdef USE_GMT4
unsigned char ggrd_grdtrack_interpolate(double *in, /* lon/lat/z [2/3] in degrees/km */
					unsigned char three_d, /* use 3-D inetrpolation or 2-D? */
					struct GRD_HEADER *grd, /* grd information */
					float *f,	/* data array */
					struct GMT_EDGEINFO *edgeinfo, /* edge information */
					int mm, /* nx * ny */
					float *z, /* depth layers */
					int nz,	/* number of depth layers */
					double *value, /* output value */
					unsigned char verbose,
					struct GMT_BCR *bcr)
#else
unsigned char ggrd_grdtrack_interpolate(double *in, /* lon/lat/z [2/3] in degrees/km */
					unsigned char three_d, /* use 3-D inetrpolation or 2-D? */
					struct GRD_HEADER *grd, /* grd information */
					float *f,	/* data array */
					struct GMT_EDGEINFO *edgeinfo, /* edge information */
					int mm, /* nx * ny */
					float *z, /* depth layers */
					int nz,	/* number of depth layers */
					double *value, /* output value */
					unsigned char verbose
					)
#endif
{
  int i1,i2;
  double fac1,fac2,val1,val2;
  /* If point is outside grd area, 
     shift it using periodicity or skip if not periodic. */
  
  while ( (in[1] < grd[0].y_min) && (edgeinfo[0].nyp > 0) ) 
    in[1] += (grd[0].y_inc * edgeinfo[0].nyp);
  if (in[1] < grd[0].y_min) 
    return FALSE;
  
  while ( (in[1] > grd[0].y_max) && (edgeinfo[0].nyp > 0) )
    in[1] -= (grd[0].y_inc * edgeinfo[0].nyp);
  if (in[1] > grd[0].y_max) 
    return FALSE;
  
  while ( (in[0] < grd[0].x_min) && (edgeinfo[0].nxp > 0) ) 
    in[0] += (grd[0].x_inc * edgeinfo[0].nxp);
  if (in[0] < grd[0].x_min) 
    return FALSE;
  
  while ( (in[0] > grd[0].x_max) && (edgeinfo[0].nxp > 0) ) 
    in[0] -= (grd[0].x_inc * edgeinfo[0].nxp);
  if (in[0] > grd[0].x_max) 
    return FALSE;
  
  /* 
     interpolate 
  */
  if(three_d){
    ggrd_gt_interpolate_z(in[2],z,nz,&i1,&i2,&fac1,&fac2,verbose);
    /* 
       we need these calls to reset the bcr.i and bcr.j counters 
       otherwise the interpolation routine would assume we have the same 
       grid
       
       TO DO:
       
       now, we still need the same grid dimensions, else a separate bcr
       variable has to be introduced
       
       
       
    */
#ifdef USE_GMT4
    val1 = GMT_get_bcr_z((grd+i1), in[0], in[1], (f+i1*mm), (edgeinfo+i1),bcr);
    val2 = GMT_get_bcr_z((grd+i2), in[0], in[1], (f+i2*mm), (edgeinfo+i2),bcr);
#else
    ggrd_gt_bcr_init_loc ();
    val1 = GMT_get_bcr_z((grd+i1), in[0], in[1], (f+i1*mm), (edgeinfo+i1));
    ggrd_gt_bcr_init_loc ();
    val2 = GMT_get_bcr_z((grd+i2), in[0], in[1], (f+i2*mm), (edgeinfo+i2));
#endif
    /*      fprintf(stderr,"z(%3i/%3i): %11g z: %11g z(%3i/%3i): %11g f1: %11g f2: %11g v1: %11g v2: %11g rms: %11g %11g\n",   */
    /* 	      i1+1,nz,z[i1],in[2],i2+1,nz,z[i2],fac1,fac2,  */
    /*        	      val1,val2,rms((f+i1*mm),mm),rms((f+i2*mm),mm));   */
    *value  = fac1 * val1;
    *value += fac2 * val2;
  }else{
#ifdef USE_GMT4
    *value = GMT_get_bcr_z(grd, in[0], in[1], f, edgeinfo,bcr);
#else
    *value = GMT_get_bcr_z(grd, in[0], in[1], f, edgeinfo);
#endif
  }
  //if(verbose)
  //fprintf(stderr,"ggrd_interpolate: lon: %g lat: %g val: %g\n",in[0],in[1],*value);
  return TRUE;
}
/*
  
  read in times for time history of velocities, if needed

  if read_thistory is TRUE:

  the input file for the tectonic stages is in format

  t_start^1 t_stop^1
  ....
  t_start^nvtimes t_stop^nvtimes
  
  expecting ascending time intervals, which should have smaller time first and have no gaps

  on return, the vtimes vector is in the format 

     t_left^1 t_mid^1 t_right^1 
     t_left^2 ...
     ...
     t_left^nvtimes t_mid^nvtimes t_right^nvtimes 
     

  else, will only init for one step
  
  returns error code 
*/
int ggrd_read_time_intervals(struct ggrd_t *thist,char *input_file,
			      unsigned char read_thistory,
			     unsigned char verbose)
{
  FILE *in;
  double ta,tb;
  if(thist->init){
    fprintf(stderr,"ggrd_read_time_intervals: error: already initialized\n");
    return 1;
  }
  hc_vecalloc(&thist->vtimes,3,"rti: 1");

  if(read_thistory){
    in = fopen(input_file,"r");
    if(!in){
      fprintf(stderr,"ggrd_read_time_intervals: error: could not open file %s\n",
	      input_file);
      return 2;
    }
    thist->nvtimes = thist->nvtimes3 = 0;
    while(fscanf(in,"%lf %lf",&ta,&tb) == 2){
      thist->vtimes[thist->nvtimes3] = ta;
      thist->vtimes[thist->nvtimes3+2] = tb;
      if(thist->nvtimes > 0){
	if((*(thist->vtimes+thist->nvtimes3+2) < *(thist->vtimes+thist->nvtimes3))||
	   (*(thist->vtimes+thist->nvtimes3) < *(thist->vtimes+(thist->nvtimes-1)*3))||
	   (*(thist->vtimes+thist->nvtimes3+2) < *(thist->vtimes+(thist->nvtimes-1)*3+2))||
	   (fabs(*(thist->vtimes+(thist->nvtimes - 1)*3+2) - *(thist->vtimes+thist->nvtimes3))>5e-7)){
	  GGRD_PE("ggrd_read_time_intervals: error, expecting ascending time intervals");
	  GGRD_PE("ggrd_read_time_intervals: which should have smaller time first and have no gaps");
	}
      }
      // compute mid point
      *(thist->vtimes + thist->nvtimes3+1) =  
	(*(thist->vtimes+thist->nvtimes3) + *(thist->vtimes + thist->nvtimes3+2))/2.0;
      thist->nvtimes += 1;
      thist->nvtimes3 += 3;
      hc_vecrealloc(&thist->vtimes,thist->nvtimes3+3,"rti: 2");
    }
    if(!(thist->nvtimes)){
      fprintf(stderr,"ggrd_read_time_intervals: error, no times read from %s\n",
	      input_file);
      return 3;
    }else{
      if(verbose){
	fprintf(stderr,"ggrd_read_time_intervals: read %i time intervals from %s\n",
		thist->nvtimes,input_file);
	fprintf(stderr,"ggrd_read_time_intervals: t_min: %g t_max: %g\n",
		*(thist->vtimes+0),*(thist->vtimes+ (thist->nvtimes-1) * 3 +2));
      }
    }
    fclose(in);
  }else{
    /* 
       only one time step 
    */
    thist->nvtimes = 1;
    thist->nvtimes3 = thist->nvtimes * 3;
    *(thist->vtimes+0) = *(thist->vtimes+1) = *(thist->vtimes+2) = 0.0;
    if(verbose)
      fprintf(stderr,"ggrd_read_time_intervals: only one timestep\n");
  }
  thist->init = TRUE;
  return 0;
}

/* 

use linear interpolation for z direction

*/
void ggrd_gt_interpolate_z(double z,float *za,int nz,
			   int *i1, int *i2, 
			   double *fac1, double *fac2,
			   unsigned char verbose)
{
  int nzm1;
  static unsigned char warned=FALSE;
  if((!warned)&&verbose){
    if((z<za[0])||(z>za[nz-1])){
      fprintf(stderr,"interpolate_z: WARNING: at least one z value extrapolated\n");
      fprintf(stderr,"interpolate_z: zmin: %g z: %g zmax: %g\n",
	      za[0],z,za[nz-1]);
      
      warned = TRUE;
    }
  }
  nzm1 = nz-1;
  *i2 = 0;
  while((za[*i2]<z)&&(*i2 < nzm1))
    *i2 += 1;
  if(*i2 == 0)
    *i2 += 1;
  *i1 = *i2 - 1;

  *fac2 = ((z - (double)za[*i1])/((double)za[*i2]-(double)za[*i1]));
  *fac1 = 1.0 - *fac2;
}

//
//     produce inetrpolation weights 
//     given a vtimes(nvtimes * 3) vector with 
//             t_left t_mid t_right 
//     entries
//
// this routine will smooth transitions between tectonic stages
//
//  INPUT:
//
//  time: time to be interpolated to
//        
//  OUTPUT:
//  i1, i2: indices of the two time intervals
//  f1, f2: weights of these intervals 
//
//
//     WARNING: if the routine gets called with the same time twice, will
//     not recalculate the weights
//
void ggrd_interpol_time(GGRD_CPREC time,struct ggrd_t *thist,
			int *i1,int *i2,GGRD_CPREC *f1,GGRD_CPREC *f2)
{
  //     dxlimit is the width of the transition between velocity stages
  static GGRD_CPREC dxlimit = 0.01,xllimit= -0.005,xrlimit=  0.005;

  //     these are the local, saved, quantities which are not determined anew
  //     if the time doesn't change
  //     
  static GGRD_CPREC f1_loc,f2_loc,time_old;
  static int ileft_loc,iright_loc;
  static unsigned char called = FALSE;
  GGRD_CPREC tloc,xll;
  int i22;
  if(!thist->init){
    fprintf(stderr,"ggrd_interpol_time: error: thist is not init\n");
    exit(-1);
  }
  tloc = time;
  //
  //     special case: only one interval
  //
  if(thist->nvtimes == 1){
    *i1 = 0;
    *i2 = 0;
    *f1 = 1.0;
    *f2 = 0.0;
  }else{ // more than one stage
    if((!called) || (fabs(tloc - time_old)>5e-7)){ 
       //     
      //     heave to determine interpolation factors, time has changed
      //     or not called at least once
      //     
      //     obtain the time intervals to the left and right of time
      //     
      if(tloc < thist->vtimes[0]){
	if(fabs(tloc - thist->vtimes[0]) < 0.1){
	  tloc = thist->vtimes[0];
	}else{
	  fprintf(stderr,"inter_vel: error, time: %g  too small\n",tloc);
	  fprintf(stderr,"inter_vel: while vtimes are given from %g to %g\n",
		  thist->vtimes[0],thist->vtimes[thist->nvtimes3-1]);
	  exit(-1);  
	}
      }
      if(tloc > thist->vtimes[thist->nvtimes3-1]){
	if(fabs(tloc - thist->vtimes[thist->nvtimes3-1]) < 0.1){
	  tloc = thist->vtimes[thist->nvtimes3-1];
	}else{
	  fprintf(stderr,"inter_vel: error, time: %g too large\n",tloc);
	  fprintf(stderr,"inter_vel: while vtimes are given from %g to %g\n",
		  thist->vtimes[0],thist->vtimes[thist->nvtimes3-1]);
	  exit(-1);
	}
      }
      iright_loc=0;  // right interval index
      i22=1;         // right interval midpoint
      //     find the right interval such that its midpoint is larger or equal than time
      while((tloc > thist->vtimes[i22]) && (iright_loc < thist->nvtimes)){
	iright_loc++;
	i22 += 3;
      }
      if(iright_loc == 0){
	iright_loc = 1;
	i22 = 4;
      }
      ileft_loc = iright_loc - 1; // left interval index
      //     distance from right boundary of left interval 
      //     (=left boundary of right interval) normalized by the mean interval width 
      xll = 2.0 * (tloc - thist->vtimes[i22-1])/(thist->vtimes[i22] - thist->vtimes[ileft_loc*3-2]);
      //     this will have xll go from -0.5 to 0.5 around the transition between stages
      //     which is at xl1=0
      //
      //     vf1_loc and vf2_loc are the weights for velocities within the left and right
      //     intervals respectively
      if(xll < xllimit){ // xllimit should be 1-dx, dx~0.1
	f1_loc = 1.0;
	f2_loc = 0.0;
      }	else {
	if(xll > xrlimit){ // xrlimit should be 1+dx, dx~0.1
	  f1_loc = 0.0;
	  f2_loc = 1.0;
	}else {            // in between 
	  xll =     (xll-xllimit)/dxlimit; // normalize by transition width
	  f2_loc = ((1.0 - cos(xll * M_PI))/2.0); // this goes from 0 to 1
	  //     weight for left velocities
	  f1_loc = 1.0 - f2_loc;
	}
      }
      called = TRUE;
      time_old = tloc;
    }
    //     assign the local variables to the output variables
    //     
    *i1 = ileft_loc;
    *i2 = iright_loc;
    *f1 = f1_loc;
    *f2 = f2_loc;
  } // end ntimes>1 part
}

/* compute simple RMS */
float ggrd_gt_rms(float *x,int n)
{
  int i;
  float rms = 0.0;
  for(i=0;i<n;i++)
    rms += x[i]*x[i];
  return sqrt(rms/(float)n);
}
/* compute simple mean */
float ggrd_gt_mean(float *x,int n)
{
  int i;
  float mean = 0.0;
  for(i=0;i<n;i++)
    mean += x[i];
  return mean/(float)n;
}
#ifndef USE_GMT4
/* 

this is aweful, but works


*/
void ggrd_gt_bcr_init_loc(void)
{
  /* Initialize i,j so that they cannot look like they have been used:  */
  bcr.i = -10;
  bcr.j = -10;
}
#endif
/*


*	Id: grdtrack.c,v 1.5.4.6 2003/04/17 22:39:25 pwessel Exp 
*
*	Copyright (c) 1991-2003 by P. Wessel and W. H. F. Smith
*	See COPYING file for copying and redistribution conditions.
*
*	This program is free software; you can redistribute it and/or modify
*	it under the terms of the GNU General Public License as published by
*	the Free Software Foundation; version 2 of the License.
*
*	This program is distributed in the hope that it will be useful,
*	but WITHOUT ANY WARRANTY; without even the implied warranty of
*	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*	GNU General Public License for more details.
*
*	Contact info: gmt.soest.hawaii.edu
*--------------------------------------------------------------------

 * grdtrack reads a xyfile, opens the 2d binary gridded grdfile, 
 * and samples the dataset at the xy positions with a bilinear or bicubic
 * interpolant.  This new data is added to the input as an extra column
 * and printed to standard output.  In order to evaluate derivatives along
 * the edges of the grdfile region, we assume natural bicubic spline
 * boundary conditions (d2z/dn2 = 0, n being the normal to the edge;
 * d2z/dxdy = 0 in the corners).  Rectangles of size x_inc by y_inc are 
 * mapped to [0,1] x [0,1] by affine transformation, and the interpolation
 * done on the normalized rectangle.
 *
 * Author:	Walter H F Smith
 * Date:	23-SEP-1993
 * 
 * Based on the original grdtrack, which had this authorship/date/history:
 *
 * Author:	Paul Wessel
 * Date:	29-JUN-1988
 * Revised:	5-JAN-1990	PW: Updated to v.2.0
 *		4-AUG-1993	PW: Added -Q
 *		14-AUG-1998	PW: GMT 3.1
 *  Modified:	10 Jul 2000 3.3.5  by PW to allow plain -L to indicate geographic coordinates
 * Version:	3.4.3
 */
