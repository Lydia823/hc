#include "hc.h"
/* 
   general routines dealing with the hager & Connell implementation

  
   $Id: hc_init.c,v 1.14 2006/03/21 08:07:18 becker Exp becker $

*/

/* 

initialize the basic operational modes
 */
void hc_init_parameters(struct hc_parameters *p)
{
  /* 
     
  operational modes and parameters

  */
  p->compressible = FALSE;		/* compressibility following Panasyuk
				   & Steinberger */
  p->free_slip = FALSE;		/* surface mechanical boundary condition */
  p->vel_bc_zero = FALSE;		/* 
				   if false, plate velocities, else no
				   slip if free_slip is false as well
				*/
  p->dens_anom_scale = 0.2;	/* default density anomaly scaling to
				   go from PREM percent traveltime
				   anomalies to density anomalies */
  p->verbose = 0;		/* debugging output? (0,1,2,3,4...) */
  p->sol_binary_out = TRUE;	/* binary or ASCII output of SH expansion */
  p->solution_mode = HC_VEL;	/* velocity, stress, or geoid */

  p->print_pt_sol = FALSE;
  p->print_spatial = FALSE;	/* by default, only print the spectral solution */
  /* 

  filenames
  
  */
  strncpy(p->visc_filename,HC_VISC_FILE,HC_CHAR_LENGTH);
  strncpy(p->dens_filename,HC_DENS_SH_FILE,HC_CHAR_LENGTH);
  strncpy(p->pvel_filename,HC_PVEL_FILE,HC_CHAR_LENGTH);
  strncpy(p->prem_model_filename,PREM_MODEL_FILE,HC_CHAR_LENGTH);
}

/* 

   first, call this routine with a blank **hc 
   
*/
void hc_struc_init(struct hcs **hc)
{
  
  *hc = (struct hcs *)calloc(1,sizeof(struct hcs *));
  if(!(*hc))
    HC_MEMERROR("hc_struc_init: hc");
  /* 
     assign NULL pointers to allow reallocating 
  */
  (*hc)->r = (*hc)->visc = (*hc)->rvisc = 
    (*hc)->dfact = (*hc)->rden = NULL;
  (*hc)->rpb = (*hc)->fpb= NULL;
  (*hc)->dens_anom = (*hc)->geoid = NULL; /* expansions */
  (*hc)->plm = NULL;
  (*hc)->prem_init = FALSE;
}
/* 

initialize all variables, after initializing the parameters 


INPUT: sh_type: type of expansion storage/spherical haronics scheme to use

INPUT: hc_parameters: holds all the settings


OUTPUT: hc structure, gets modified
*/
void hc_init_main(struct hcs *hc,int sh_type,
		  struct hc_parameters *p)
{
  int dummy=0;
  /* mechanical boundary condition */
  hc->free_slip = p->free_slip;
  /* 
     set the default expansion type, input expansions will be 
     converted 
  */
  hc->sh_type = sh_type;
  /* 
     start by reading in physical constants and PREM model
  */
  hc_init_constants(hc,p->dens_anom_scale,
		    p->prem_model_filename,p->verbose);
  /* 
     initialize viscosity structure from file
  */
  hc_assign_viscosity(hc,HC_INIT_FROM_FILE,
		      p->visc_filename,p->verbose);
  if(p->vel_bc_zero){
    /* no slip (zero velocity) surface boundary conditions */
    if(p->free_slip)
      HC_ERROR("hc_init","vel_bc_zero and free_slip doesn't make sense");
    /* read in the densities */
    hc_assign_density(hc,p->compressible,HC_INIT_FROM_FILE,
		      p->dens_filename,-1,FALSE,FALSE,p->verbose);
    /* assign all zeroes up to the lmax of the density expansion */
    hc_assign_plate_velocities(hc,HC_INIT_FROM_FILE,
			       p->pvel_filename,
			       p->vel_bc_zero,
			       hc->dens_anom[0].lmax,
			       FALSE,p->verbose);
  }else{
    /* presribed surface velocities */
    if(!p->free_slip){
      /* read in velocities, which will determine the solution lmax */
      hc_assign_plate_velocities(hc,HC_INIT_FROM_FILE,
				 p->pvel_filename,
				 p->vel_bc_zero,
				 dummy,FALSE,p->verbose);
      hc_assign_density(hc,p->compressible,HC_INIT_FROM_FILE,
			p->dens_filename,hc->pvel[0].lmax,
			FALSE,FALSE,p->verbose);
    }else{
      if(p->verbose)
	fprintf(stderr,"hc_init: initializing for free-slip\n");
      /* read in the density fields */
      hc_assign_density(hc,p->compressible,HC_INIT_FROM_FILE,
			p->dens_filename,-1,FALSE,FALSE,
			p->verbose);
    }
  }
  /* 
     phase boundaries, if any 
  */
  hc_init_phase_boundaries(hc,0,p->verbose);
  /*  */
  hc->save_solution = TRUE;	/* (don')t save the propagator
				   matrices in hc_polsol and the
				   poloidal/toroidal solutions
				*/
  hc->initialized = TRUE;
}
/* 

   some of those numbers might be a bit funny, but leave them like
   this for now for backward compatibility.

*/
void hc_init_constants(struct hcs *hc, HC_PREC dens_anom_scale,
		       char *prem_filename,hc_boolean verbose)
{
  static hc_boolean init=FALSE;
  int ec;
  if(init)
    HC_ERROR("hc_init_constants","why would you call this routine twice?")
  if(!hc->prem_init){
    /* PREM constants */
    if((ec=prem_read_model(prem_filename,hc->prem,verbose))){
      fprintf(stderr,"hc_init_constants: error: could not init PREM, error code: %i\n",
	      ec);
      exit(-1);
    }
    hc->prem_init = TRUE;
  }
  /* 
     density scale 
  */
  hc->dens_scale[0] = dens_anom_scale;
  /* 
     constants
  */
  hc->timesc = HC_TIMESCALE_YR;		/* timescale [yr]*/
  hc->visnor = 1e21;		/* normalizing viscosity [Pas]*/
  hc->gacc = 10.0e2; 		/* gravitational acceleration [cm/s2]*/
  hc->g = 6.6742e-11;		/* gravitational constant [Nm2/kg2]*/
  /*  
      radius of Earth in [m]
  */
  hc->re = hc->prem->r0;
  if(fabs(hc->re - (HC_RE_KM * 1e3)) > 1e-7)
    HC_ERROR("hc_init_constants","Earth radius mismatch")
  hc->secyr = 3.1556926e7;	/* seconds/year  */
  /* 
     those are in g/cm^3
  */
  hc->avg_den_mantle = 4.4488;
  hc->avg_den_core = 11.60101;


  /* 
     take the CMB radius from the Earth model 
  */
  hc->r_cmb = hc->prem->r[1];
  if(fabs(hc->r_cmb - 0.55) > 0.02)
    HC_ERROR("hc_init_constants","Earth model CMB radius appears off");
  /* velocity scale if input is in [cm/yr], 
     works out to be ~0.11 */
  hc->vel_scale = hc->re*PIOVERONEEIGHTY/hc->timesc;
  init = TRUE;
}

/* 
   
     handle command line  parameters
     
     visc_filename[] needs to be [HC_CHAR_LENGTH]

 */
void hc_handle_command_line(int argc, char **argv,
			    struct hc_parameters *p)
{
  int i;
  for(i=1;i < argc;i++){
    if(strcmp(argv[i],"-h")==0 || strcmp(argv[i],"-?")==0){// help
      /* 
	 help page
      */
      fprintf(stderr,"%s - perform Hager & O'Connell flow computation\n\n",
	      argv[0]);
      fprintf(stderr,"options:\n\n");
      fprintf(stderr,"-dens\tname\tuse name as a SH density anomaly model (%s)\n",
	      p->dens_filename);
      fprintf(stderr,"-ds\t\tdensity scaling (%g)\n",
	      p->dens_anom_scale);
      fprintf(stderr,"-fs\t\tperform free slip computation, else no slip or plates (%s)\n",
	      hc_name_boolean(p->free_slip));
      fprintf(stderr,"-ns\t\tperform no slip computation, else try to read plate velocities (%s)\n",
	      hc_name_boolean(p->vel_bc_zero));
      fprintf(stderr,"-prem\tname\tset Earth model to name (%s)\n",
	      p->prem_model_filename);
      fprintf(stderr,"-pptsol\t\tprint pol[6] and tor[2] solution vectors (%s)\n",
	      hc_name_boolean(p->print_pt_sol));
      fprintf(stderr,"-pvel\tname\tset surface velocity file to name (%s)\n",
	      p->pvel_filename);
      fprintf(stderr,"-px\t\tprint the spatial solution to file (%s)\n",
	      hc_name_boolean(p->print_spatial));
      fprintf(stderr,"-vf\tname\tset viscosity structure filename to name (%s)\n",
	      p->visc_filename);
      fprintf(stderr,"-v\t-vv\t-vvv: verbosity levels (%i)\n",
	      (int)(p->verbose));
      fprintf(stderr,"-visc\tname\tuse name as a viscosity structure (%s)\n",
	      p->visc_filename);
      fprintf(stderr,"\n\n");
      exit(-1);
    }else if(strcmp(argv[i],"-ds")==0){	/* density anomaly scaling factor */
      hc_advance_argument(&i,argc,argv);
      sscanf(argv[i],HC_FLT_FORMAT,&p->dens_anom_scale);
    }else if(strcmp(argv[i],"-fs")==0){	/* free slip flag */
      hc_toggle_boolean(&p->free_slip);
    }else if(strcmp(argv[i],"-ns")==0){	/* no slip flag */
      hc_toggle_boolean(&p->vel_bc_zero);
    }else if(strcmp(argv[i],"-px")==0){	/* print spatial solution? */
      hc_toggle_boolean(&p->print_spatial);
    }else if(strcmp(argv[i],"-pptsol")==0){	/* print
						   poloidal/toroidal
						   solution
						   parameters */
      hc_toggle_boolean(&p->print_pt_sol);
    }else if(strcmp(argv[i],"-vf")==0){ /* viscosity filename */
      hc_advance_argument(&i,argc,argv);
      strncpy(p->visc_filename,argv[i],HC_CHAR_LENGTH);
    }else if(strcmp(argv[i],"-prem")==0){ /* PREM filename */
      hc_advance_argument(&i,argc,argv);
      strncpy(p->prem_model_filename,argv[i],HC_CHAR_LENGTH);
    }else if(strcmp(argv[i],"-pvel")==0){ /* velocity filename */
      hc_advance_argument(&i,argc,argv);
      strncpy(p->pvel_filename,argv[i],HC_CHAR_LENGTH);
    }else if(strcmp(argv[i],"-dens")==0){ /* density filename */
      hc_advance_argument(&i,argc,argv);
      strncpy(p->dens_filename,argv[i],HC_CHAR_LENGTH);
    }else if(strcmp(argv[i],"-visc")==0){ /* viscosity filename */
      hc_advance_argument(&i,argc,argv);
      strncpy(p->visc_filename,argv[i],HC_CHAR_LENGTH);
    }else if(strcmp(argv[i],"-v")==0){	/* verbosities */
      p->verbose = 1;
    }else if(strcmp(argv[i],"-vv")==0){	/* verbosities */
      p->verbose = 2;
    }else if(strcmp(argv[i],"-vvv")==0){	
      p->verbose = 3;
    }else{
      fprintf(stderr,"%s: can not use parameter %s, use -h for help page\n",
	      argv[0],argv[i]);
      exit(-1);
    }
  }
}
/* 

assign viscosity structure

mode == 0

read in a viscosity structure with layers of constant viscosity in
format

r visc 

where r is non-dim radius and visc non-dim viscosity, r has to be
ascending

*/
void hc_assign_viscosity(struct hcs *hc,int mode,char filename[HC_CHAR_LENGTH],
			 hc_boolean verbose)
{
  FILE *in;
  char fstring[100];
  HC_PREC mean,mweight,rold,mws;
  static hc_boolean init=FALSE;
  switch(mode){
  case HC_INIT_FROM_FILE:		
    /* 
       
       init from file part 
    
    */
    if(init)
      HC_ERROR("hc_assign_viscosity","viscosity already read from file, really read again?");
    /* 
       read viscosity structure from file 

       format:

       r[non-dim] visc[non-dim]

       from bottom to top
    */
    in = hc_open(filename,"r","hc_assign_viscosity");
    hc_vecrealloc(&hc->rvisc,1,"hc_assign_viscosity");
    hc_vecrealloc(&hc->visc,1,"hc_assign_viscosity");
    hc->nvis = 0;mean = 0.0;mws = 0.0;
    /* read sscanf string */
    hc_get_flt_frmt_string(fstring,2,FALSE);
    rold = hc->r_cmb;
    /* start read loop  */
    while(fscanf(in,"%lf %lf",
		 (hc->rvisc+hc->nvis),(hc->visc+hc->nvis))==2){
      if(hc->nvis == 0)
	if( hc->rvisc[hc->nvis] < hc->r_cmb-0.01){
	  fprintf(stderr,"hc_assign_viscosity: error: first radius %g is below CMB, %g\n",
		  hc->rvisc[hc->nvis], hc->r_cmb);
	  exit(-1);
	}
      if(verbose){
	/* weighted mean, should use volume, really, but this is just
	   for information  */
	mweight = ( hc->rvisc[hc->nvis] - rold); 
	mws += mweight;
	rold = hc->rvisc[hc->nvis];
	mean += log(hc->visc[hc->nvis]) * mweight;
      }
      if(hc->nvis){
	if(hc->rvisc[hc->nvis] < hc->rvisc[hc->nvis-1]){
	  fprintf(stderr,"hc_assign_viscosity: error: radius has to be ascing, entry %i (%g) smaller than last (%g)\n",
		  hc->nvis+1,hc->rvisc[hc->nvis],hc->rvisc[hc->nvis-1]);
	  exit(-1);
	}
      }
      hc->nvis++;
      hc_vecrealloc(&hc->rvisc,hc->nvis+1,"hc_assign_viscosity");
      hc_vecrealloc(&hc->visc,hc->nvis+1,"hc_assign_viscosity");
    }
    fclose(in);
    if(hc->rvisc[hc->nvis-1] > 1.0){
      fprintf(stderr,"hc_assign_viscosity: error: first last %g is above surface, 1.0\n",
	      hc->rvisc[hc->nvis-1]);
      exit(-1);
    }
    if(verbose){
      /* last entry */
      mweight = ( 1.0 - hc->rvisc[hc->nvis-1]); 
      mws += mweight;
      rold = hc->rvisc[hc->nvis-1];
      mean += log(hc->visc[hc->nvis-1]) * mweight;
      
      mean = exp(mean/mws);
      fprintf(stderr,"hc_assign_viscosity: read %i layers of non-dimensionalized viscosities from %s\n",
	      hc->nvis,filename);
      fprintf(stderr,"hc_assign_viscosity: rough estimate of mean viscosity: %g x %g = %g Pas\n",
	      mean, hc->visnor, mean*hc->visnor);
    }
    break;
  default:
    HC_ERROR("hc_assign_viscosity","mode undefined");
    break;
  }
  init = TRUE;
}
/* 

assign/initialize the density anomalies and density factors

if mode==0: expects spherical harmonics of density anomalies [%] with
            respect to the 1-D reference model (PREM) given in SH
            format on decreasing depth levels in [km]
	    

	    spherical harmonics are real, fully normalized as in 
	    Dahlen & Tromp p. 859


this routine assigns the inho density radii, and the total (nrad=inho)+2
radii 

furthermore, the dfact factors are assigned as well

set  density_in_binary to TRUE, if expansion given in binary

nominal_lmax: -1: the max order of the density expansion will either
                  determine the lmax of the solution (free-slip, or vel_bc_zero) or 
		  will have to be the same as the plate expansion lmax (!free_slip && !vel_bc_zero)
              else: will zero out all entries > nominal_lmax

*/
void hc_assign_density(struct hcs *hc,
		       hc_boolean compressible,int mode,
		       char *filename,int nominal_lmax,
		       hc_boolean layer_structure_changed,
		       hc_boolean density_in_binary,
		       hc_boolean verbose)
{
  FILE *in;
  int type,lmax,shps,ilayer,nset,ivec,i,j;
  HC_PREC *dtop,*dbot,zlabel,dens_scale[1],rho0;
  hc_boolean reported = FALSE;
  static HC_PREC local_dens_fac = .01;	/* this factor will be
					   multiplied with the
					   hc->dens_fac factor to
					   arrive at fractional
					   anomalies from input. set
					   to 0.01 for percent input,
					   for instance
					*/
  static hc_boolean init=FALSE;
  hc->compressible = compressible;

  if(init)			/* clear old expansions, if 
				   already initialized */
    sh_free_expansion(hc->dens_anom,hc->inho);
  /* get PREM model, if not initialized */
  if(!hc->prem_init)
    HC_ERROR("hc_assign_density","assign 1-D reference model (PREM) first");
  switch(mode){
  case HC_INIT_FROM_FILE:
    if(init)
      HC_ERROR("hc_assign_density","really read dens anomalies again from file?");

    /* 
       
    read in density anomalies in spherical harmonics format for
    different layers from file. 

    this assumes that we are reading in anomalies in percent
    
    */
    in = hc_open(filename,"r","hc_assign_density");
    if(verbose)
      fprintf(stderr,"hc_assign_density: reading density anomalies in [%%] from %s, scaling by %g\n",
	      filename,hc->dens_scale[0]);
    hc->inho = 0;		/* counter for density layers */
    /* get one density expansion */
    hc_get_blank_expansions(&hc->dens_anom,1,0,"hc_assign_density");
    /* 
       read all layes as spherical harmonics assuming real Dahlen & Tromp 
       (physical) normalization

    */
    while(sh_read_parameters(&type,&lmax,&shps,&ilayer, &nset,
			     &zlabel,&ivec,in,FALSE,density_in_binary,
			     verbose)){
      if((verbose)&&(!reported)){
	if(nominal_lmax > lmax)
	  fprintf(stderr,"hc_assign_density: density lmax: %3i filling up to nominal lmax: %3i with zeroes\n",
		  lmax,nominal_lmax);
	if(nominal_lmax != -1){
	  fprintf(stderr,"hc_assign_density: density lmax: %3i limiting to lmax: %3i\n",
		  lmax,nominal_lmax);
	}else{
	  fprintf(stderr,"hc_assign_density: density lmax: %3i determines solution lmax\n",
		  lmax);
	}
	reported = TRUE;
      }
      /* 
	 do tests 
      */
      if((shps != 1)||(ivec))
	HC_ERROR("hc_assign_density","vector field read in but only scalar expansion expected");
      /* test and assign depth levels */
      hc->rden=(HC_PREC *)
	realloc(hc->rden,(1+hc->inho)*sizeof(HC_PREC));
      if(!hc->rden)
	HC_MEMERROR("hc_assign_density: rden");
      /* 
	 assign depth, this assumes that we are reading in depths [km]
      */
      hc->rden[hc->inho] = HC_ND_RADIUS(zlabel);
      /* 

      get reference density at this level

      */
      prem_get_rho(&rho0,hc->rden[hc->inho],hc->prem);
      /* 
	 general density (add additional depth dependence here)
      */
      dens_scale[0] = hc->dens_scale[0] * local_dens_fac * rho0/1000.;
      if(verbose >= 2)
	fprintf(stderr,"hc_assign_density: r: %11g anom scales: %11g x %11g x %11g = %11g\n",
		hc->rden[hc->inho],hc->dens_scale[0],
		local_dens_fac,rho0,dens_scale[0]);
      if(hc->inho){	
	/* 
	   check by comparison with previous expansion 
	*/
	if(nominal_lmax == -1)
	  if(lmax != hc->dens_anom[0].lmax)
	    HC_ERROR("hc_assign_density","lmax changed in file");
	if(hc->rden[hc->inho] <= hc->rden[hc->inho-1])
	  HC_ERROR("hc_assign_density","depth should decrease, radius increase (give z[km])");
      }
      /* 
	 make room for new expansion 
      */
      hc_get_blank_expansions(&hc->dens_anom,hc->inho+1,hc->inho,
			      "hc_assign_density");
      /* 
	 initialize expansion
      */
      sh_init_expansion((hc->dens_anom+hc->inho),
			(nominal_lmax > lmax) ? (nominal_lmax):(lmax),
			hc->sh_type,1,verbose);
      /* 
	 
	 read parameters and scale (put possible depth dependence of
	 scaling here)
	 
	 will assume input is in physical convention

      */
      sh_read_coefficients((hc->dens_anom+hc->inho),1,lmax,
			   in,density_in_binary,dens_scale,
			   verbose);
      hc->inho++;
    }
    if(hc->inho != nset)
      HC_ERROR("hc_assign_density","file mode: mismatch in number of layers");
    fclose(in);
    break;
  default:
    HC_ERROR("hc_assign_density","mode undefined");
    break;
  }
  if((!init)||(layer_structure_changed)){
    /* 
       
    assign the other radii, nrad + 2
    
    */
    hc->nrad = hc->inho;
    hc_vecrealloc(&hc->r,(2+hc->nrad),"hc_assign_density");
    hc->r[0] = hc->r_cmb;	/* CMB  */
    if(hc->rden[0] <= hc->r[0])
      HC_ERROR("hc_assign_density","first density layer has to be above internal CMD limit");
    for(i=0;i<hc->nrad;i++)	/* density layers */
      hc->r[i+1] = hc->rden[i];
    if(hc->rden[hc->nrad-1] >= 1.0)
      HC_ERROR("hc_assign_density","uppermost density layer has to be below surface");
    hc->r[hc->nrad+1] = 1.0;	/* surface */
    /* 

    assign the density jump factors

    */
    /* 
       since we have spherical harmonics at several layers, we assign 
       the layer thickness by picking the intermediate depths
    */
    hc_vecalloc(&dbot,hc->nrad,"hc_assign_density");
    hc_vecalloc(&dtop,hc->nrad,"hc_assign_density");
    //    top boundaries
    j = hc->nrad-1;
    for(i=0;i < j;i++)
      dtop[i] = 1.0 - (hc->rden[i+1] + hc->rden[i])/2.0;
    dtop[j] = 0.0; // top boundary
    //    bottom boundaries
    dbot[0] = 1.0 - hc->r_cmb;  // bottom boundary, ie. CMB 
    for(i=1;i < hc->nrad;i++)
      dbot[i] = dtop[i-1];
    /* 
       density layer thickness factors
    */
    hc_dvecrealloc(&hc->dfact,hc->nrad,"hc_assign_density");
    for(i=0;i<hc->nrad;i++){
      hc->dfact[i] = 1.0/hc->rden[i] *(dbot[i] - dtop[i]);
    }
    if(verbose)
      for(i=0;i < hc->nrad;i++)
	fprintf(stderr,"hc_assign_density: dens %3i: r: %8.6f df: %8.6f |rho|: %8.4f\n",
		i+1,hc->rden[i],hc->dfact[i],
		sqrt(sh_total_power((hc->dens_anom+i))));
    free(dbot);free(dtop);
  } /* end layer structure part */
  init = TRUE;
}
/* 

assign phase boundary jumps
input:

npb: number of phase boundaries

....



*/
void hc_init_phase_boundaries(struct hcs *hc, int npb,
			      hc_boolean verbose)
{

  hc->npb = npb;		/* no phase boundaries for now */
  if(hc->npb){
    HC_ERROR("hc_init_phase_boundaries","phase boundaries not implemented yet");
    hc_vecrealloc(&hc->rpb,hc->npb,"hc_init_phase_boundaries");
    hc_vecrealloc(&hc->fpb,hc->npb,"hc_init_phase_boundaries");
  }

}

/* 

read in plate velocities, 

vel_bc_zero: if true, will set all surface velocities to zero

lmax will only be referenced if all velocities are supposed to be set to zero

we expect velocities to be in cm/yr, convert to m/yr

*/

void hc_assign_plate_velocities(struct hcs *hc,int mode, 
				char *filename,
				hc_boolean vel_bc_zero,int lmax,
				hc_boolean pvel_in_binary,
				hc_boolean verbose)
{
  static hc_boolean init = FALSE;
  int type,shps,ilayer,nset,ivec;
  HC_PREC zlabel,vfac[2],t10[2],t11[2];
  FILE *in;
  /* scale to go from cm/yr to internal scale */
  vfac[0] = vfac[1] = hc->vel_scale;
  if(init)
    HC_ERROR("hc_assign_plate_velocities","what to do if called twice?");
  if(!vel_bc_zero){
    /* 

    velocities are NOT all zero


    */
    switch(mode){
    case HC_INIT_FROM_FILE:
      /* 
	 read velocities in pol/tor expansion format from file 
	 in units of HC_VELOCITY_FILE_FACTOR per year
      */
      if(verbose)
	fprintf(stderr,"hc_assign_plate_velocities: expecting [cm/yr] pol/tor from %s\n",
		filename);
      in = hc_open(filename,"r","hc_assign_plate_velocities");
      if(!sh_read_parameters(&type,&lmax,&shps,&ilayer, &nset,
			     &zlabel,&ivec,in,FALSE,
			     pvel_in_binary,verbose)){
	fprintf(stderr,"hc_assign_plate_velocities: read error file %s\n",
		filename);
	exit(-1);
      } /* check if we read in two sets of expansions */
      if(shps != 2){
	fprintf(stderr,"hc_assign_plate_velocities: two sets expected but found shps: %i in file %s\n",
		shps,filename);
	exit(-1);
      }
      if((nset > 1)||(fabs(zlabel) > 0.01)){
	fprintf(stderr,"hc_assign_plate_velocities: error: expected one layer at surface, but nset: %i z: %g\n",
		nset, zlabel);
	exit(-1);
      }
      /* 
	 initialize expansion 
      */
      sh_init_expansion((hc->pvel+0),lmax,hc->sh_type,1,verbose);
      sh_init_expansion((hc->pvel+1),lmax,hc->sh_type,1,verbose);
      /* 
	 read in expansions, convert to internal format from 
	 physical 
      */
      sh_read_coefficients(hc->pvel,shps,-1,in,pvel_in_binary,
			   vfac,verbose);
      fclose(in);
      /* 
	 scale by 1/sqrt(l(l+1))
      */
      if(hc->pvel[0].lmax > hc->lfac_init)
	hc_init_l_factors(hc,hc->pvel[0].lmax);
      sh_scale_expansion_l_factor((hc->pvel+0),hc->ilfac);
      sh_scale_expansion_l_factor((hc->pvel+1),hc->ilfac);
      /*  
	  check for net rotation
      */
      sh_get_coeff((hc->pvel+1),1,0,0,TRUE,t10);
      sh_get_coeff((hc->pvel+1),1,0,2,TRUE,t11);
      if(fabs(t10[0])+fabs(t11[0])+fabs(t11[1]) > 1.0e-7)
	fprintf(stderr,"\nhc_assign_plate_velocities: WARNING: toroidal A(1,0): %g A(1,1): %g B(1,1): %g\n\n",
		t10[0],t11[0],t11[1]);
      if(verbose)
	fprintf(stderr,"hc_assign_plate_velocities: read velocities, lmax %i: |pol|: %11g |tor|: %11g\n",
		lmax,sqrt(sh_total_power((hc->pvel+0))),sqrt(sh_total_power((hc->pvel+1))));
      break;
    default:
      HC_ERROR("hc_assign_plate_velocities","op mode undefined");
    }
  }else{
    /* 
       initialize with zeroes
    */
    if(init){
      sh_clear_alm(hc->pvel);
      sh_clear_alm((hc->pvel+1));
    }else{
      sh_init_expansion(hc->pvel,lmax,hc->sh_type, 
			1,verbose);
      sh_init_expansion((hc->pvel+1),lmax,hc->sh_type, 
			1,verbose);
    }
    if(verbose)
      fprintf(stderr,"hc_assign_plate_velocities: using no-slip surface BC, lmax %i\n",
	      lmax);
  }
  init = TRUE;
}
 
/* 

initialize an array with sqrt(l(l+1)) factors
from l=0 .. lmax+1

pass lfac initialized (say, as NULL)

*/
void hc_init_l_factors(struct hcs *hc, int lmax)
{
  int lmaxp1,l;
  lmaxp1 = lmax + 1;
  hc_vecrealloc(&hc->lfac,lmaxp1,"hc_init_l_factors");
  hc_vecrealloc(&hc->ilfac,lmaxp1,"hc_init_l_factors");
  /* maybe optimize later */
  hc->lfac[0] = 0.0;
  hc->ilfac[0] = 1.0;		/* shouldn't matter */
  for(l=1;l < lmaxp1;l++){
    hc->lfac[l] = sqrt((HC_PREC)l * ((HC_PREC)l + 1.0));
    hc->ilfac[l] = 1.0/hc->lfac[l];
  }
  hc->lfac_init = lmax;
}

/* 
   
reallocate new spherical harmonic expansion structures 
and initialize them with zeroes

input: 
expansion: expansion **
nold: number of old expansions
nnew: number of new expansions

*/
void hc_get_blank_expansions(struct sh_lms **expansion,
			     int nnew,int nold,
			     char *calling_sub)
{
  struct sh_lms *tmpzero;
  int ngrow;
  ngrow= nnew - nold;
  if(ngrow <= 0){
    fprintf(stderr,"hc_get_blank_expansions: error: ngrow needs to be > 0, ngrow: %i\n",
	    ngrow);
    fprintf(stderr,"hc_get_blank_expansions: was called from %s\n",
	    calling_sub);
    exit(-1);
  }
  /* 
     reallocate space 
  */
  *expansion  = (struct sh_lms *)
    realloc(*expansion,sizeof(struct sh_lms)*nnew);
  if(!(*expansion)){
    fprintf(stderr,"hc_get_blank_expansions: memory error: ngrow: %i\n",
	    nnew-nold);
    fprintf(stderr,"hc_get_blank_expansions: was called from %s\n",
	    calling_sub);
    exit(-1);
  }
  /* zero out new space */
  tmpzero  = (struct sh_lms *)calloc(ngrow,sizeof(struct sh_lms));
  if(!tmpzero)
    HC_MEMERROR("hc_get_blank_expansions: tmpzero");
  /* copy zeroes over */
  memcpy((*expansion+nold),tmpzero,ngrow*sizeof(struct sh_lms));
  free(tmpzero);
}
