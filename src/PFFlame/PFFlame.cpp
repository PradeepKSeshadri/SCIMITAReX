#include <AMReX_ParallelDescriptor.H>
#include <AMReX_ParmParse.H>
#include <AMReX_MultiFabUtil.H>
#include <AMReX_FillPatchUtil.H>

#include <AMReX_PhysBCFunct.H>

#include <PFFlame.H>
#include <PFFlameBC.H>

using namespace amrex;

PFFlame::PFFlame ()
{
  //
  // READ INPUT PARAMETERS
  //
  
  { 
    ParmParse pp;   // Basic run parameters
    pp.query("max_step", max_step);
    pp.query("stop_time", stop_time);
    pp.query("timestep",timestep);
  }

  {
    ParmParse pp("amr"); // AMR specific parameters
    pp.query("regrid_int", regrid_int);
    pp.query("check_file", check_file);
    pp.query("check_int", check_int);
    pp.query("plot_file", plot_file);
    pp.query("plot_int", plot_int);
    pp.query("restart", restart_chkfile);
  }

  {
    ParmParse pp("physics"); // Phase-field model parameters
    pp.query("M",M);
    pp.query("kappa",kappa);
    pp.query("w1",w1);
    pp.query("w12",w12);
    pp.query("w0",w0);
    pp.query("rho1",rho1);
    pp.query("rho0",rho0);
    pp.query("k1",k1);
    pp.query("k0",k0);
    pp.query("cp1",cp1);
    pp.query("cp0",cp0);
    pp.query("qdotburn",qdotburn);


    // pp.query("L", L);
    // pp.query("mu", mu);
    // pp.query("gamma", gamma);
    // pp.query("kappa", kappa);
  }

  int nlevs_max = maxLevel() + 1;

  istep.resize(nlevs_max, 0);
  nsubsteps.resize(nlevs_max, 1);
  for (int lev = 1; lev <= maxLevel(); ++lev) {
    nsubsteps[lev] = MaxRefRatio(lev-1);
  }

  t_new.resize(nlevs_max, 0.0);
  t_old.resize(nlevs_max, -1.e100);
  dt.resize(nlevs_max, 1.e100);
  for (int i = 0; i < nlevs_max; i++)
    dt[i] = timestep;

  phi_new.resize(number_of_fabs + 1); // <-- resize for number of order parameters
  phi_old.resize(number_of_fabs + 1); //
  for (int n = 0; n < number_of_fabs; n++)
    {
      phi_new[n].resize(nlevs_max);
      phi_old[n].resize(nlevs_max);
    }


  // Voronoi
  voronoi_x.resize(number_of_components);
  voronoi_y.resize(number_of_components);
  if(ParallelDescriptor::IOProcessor())
    {
      for (int n = 0; n<number_of_components; n++)
  	{
  	  voronoi_x[n] = geom[0].ProbLo(0) +
	    (geom[0].ProbHi(0)-geom[0].ProbLo(0))*((amrex::Real)rand())/((amrex::Real)RAND_MAX);
  	  voronoi_y[n] = geom[0].ProbLo(1) +
	    (geom[0].ProbHi(1)-geom[0].ProbLo(1))*((amrex::Real)rand())/((amrex::Real)RAND_MAX);
  	}
    }  
  ParallelDescriptor::Barrier("Distributing voronoi information");
  for (int n = 0; n<number_of_components; n++)
    {
      ParallelDescriptor::Bcast<amrex::Real>(&voronoi_x[n],sizeof(amrex::Real));
      ParallelDescriptor::Bcast<amrex::Real>(&voronoi_y[n],sizeof(amrex::Real));
    }

}

// PFFlame::~PFFlame ()
// {
  
// }

// void
// PFFlame::MakeNewLevelFromCoarse (int lev, Real time, const BoxArray& ba,
// 				const DistributionMapping& dm)
// {
//   for (int n = 0; n < number_of_fabs; n++)
//     {
//       const int ncomp = phi_new[n][lev-1]->nComp();
//       const int nghost = phi_new[n][lev-1]->nGrow();

//       phi_new[n][lev].reset(new MultiFab(ba, dm, ncomp, nghost));
//       phi_old[n][lev].reset(new MultiFab(ba, dm, ncomp, nghost));

//       t_new[lev] = time;
//       t_old[lev] = time - 1.e200;

//       FillCoarsePatch(lev, time, *phi_new[n][lev], 0, ncomp);
//     }
// }


// void
// PFFlame::RemakeLevel (int lev, Real time, const BoxArray& ba,
// 		     const DistributionMapping& dm)
// {
//   amrex::Array<std::unique_ptr<MultiFab> > new_state(number_of_fabs); 
//   amrex::Array<std::unique_ptr<MultiFab> > old_state(number_of_fabs);
//   for (int n=0; n < number_of_fabs; n++)
//     {
//       const int ncomp = phi_new[n][lev]->nComp();
//       const int nghost = phi_new[n][lev]->nGrow();

//       new_state[n].reset(new MultiFab(ba, dm, ncomp, nghost));
//       old_state[n].reset(new MultiFab(ba, dm, ncomp, nghost));
//       //std::unique_ptr<MultiFab> new_state(new MultiFab(ba, dm, ncomp, nghost));
//       //std::unique_ptr<MultiFab> old_state(new MultiFab(ba, dm, ncomp, nghost));
//     }

//   FillPatch(lev, time, new_state, 0);

//   for (int n=0; n < number_of_fabs; n++)
//     {
//       std::swap(new_state[n], phi_new[n][lev]);
//       std::swap(old_state[n], phi_old[n][lev]);

//       t_new[lev] = time;
//       t_old[lev] = time - 1.e200;
//     }
// }

// void
// PFFlame::ClearLevel (int lev)
// {
//   for (int n = 0; n < number_of_fabs; n++)
//     {
//       phi_new[n][lev].reset(nullptr);
//       phi_old[n][lev].reset(nullptr);
//     }
// }

 // long
 // PFFlame::CountCells (int lev)
 // {
 //   const int N = grids[lev].size();

 //   long cnt = 0;
 //   for (int i = 0; i < N; ++i)
 //     {
 //       cnt += grids[lev][i].numPts();
 //     }

 //   return cnt;
 // }

// void
// PFFlame::FillPatch (int lev, Real time, Array<std::unique_ptr<MultiFab> >& mf, int icomp)
// {
//   if (lev == 0)
//     {
//       Array<Array<MultiFab*> > smf;
//       smf.resize(number_of_fabs);
//       Array<Real> stime;
//       GetData(0, time, smf, stime);

//       PFFlamePhysBC physbc(geom[lev]);
//       for (int n = 0; n<number_of_fabs; n++)
// 	{
// 	  amrex::FillPatchSingleLevel(*mf[n], time, smf[n], stime, 0, icomp, mf[n]->nComp(),
// 				      geom[lev], physbc);
// 	}
//     }
//   else
//     {
//       Array<Array<MultiFab*> > cmf, fmf;
//       cmf.resize(number_of_fabs);
//       fmf.resize(number_of_fabs);
//       Array<Real> ctime, ftime;
//       GetData(lev-1, time, cmf, ctime);
//       GetData(lev  , time, fmf, ftime);

//       PFFlamePhysBC cphysbc(geom[lev]), fphysbc(geom[lev]);
//       Interpolater* mapper = &cell_cons_interp;

//       int lo_bc[] = {EXT_DIR, INT_DIR, INT_DIR}; // non-periodic boundaries
//       int hi_bc[] = {EXT_DIR, INT_DIR, INT_DIR};

//       //int lo_bc[] = {INT_DIR, INT_DIR, INT_DIR}; // periodic boundaries
//       //int hi_bc[] = {INT_DIR, INT_DIR, INT_DIR};
//       //Array<BCRec> bcs(1, BCRec(lo_bc, hi_bc));

//       for (int n = 0; n<number_of_fabs; n++)
// 	{
// 	  Array<BCRec> bcs(mf[n]->nComp(), BCRec(lo_bc, hi_bc)); // todo
// 	  amrex::FillPatchTwoLevels(*mf[n], time, cmf[n], ctime, fmf[n], ftime,
// 				    0, icomp, mf[n]->nComp(), geom[lev-1], geom[lev],
// 				    cphysbc, fphysbc, refRatio(lev-1),
// 				    mapper, bcs);
// 	}
//     }
// }

// void
// PFFlame::FillCoarsePatch (int lev, Real time, MultiFab& mf, int icomp, int ncomp)
// {
//   BL_ASSERT(lev > 0);

//   Array<Array<MultiFab*> > cmf(number_of_fabs);
//   Array<Real> ctime;
//   GetData(lev-1, time, cmf, ctime);
    
//   for (int n = 0; n < number_of_fabs; n++)
//     if (cmf.size() != 1) 
//       amrex::Abort("FillCoarsePatch: how did this happen?");


//   PFFlamePhysBC cphysbc(geom[lev]), fphysbc(geom[lev]);
//   //PhysBCFunct cphysbc, fphysbc;
//   Interpolater* mapper = &cell_cons_interp;
    
//   int lo_bc[] = {INT_DIR, INT_DIR, INT_DIR}; // periodic boundaries
//   int hi_bc[] = {INT_DIR, INT_DIR, INT_DIR};
//   //Array<BCRec> bcs(1, BCRec(lo_bc, hi_bc));

//   for (int n = 0; n < number_of_fabs; n++)
//     {
//       Array<BCRec> bcs(ncomp, BCRec(lo_bc, hi_bc));
//       amrex::InterpFromCoarseLevel(mf, time, *cmf[n][0], 0, icomp, ncomp, geom[lev-1], geom[lev],
// 				   cphysbc, fphysbc, refRatio(lev-1),
// 				   mapper, bcs);
//     }
// }
 
// void
// PFFlame::GetData (int lev, Real time, Array<Array<MultiFab*> >& data, Array<Real>& datatime)
// {
//   for (int n = 0; n < number_of_fabs; n++) data[n].clear();
//   datatime.clear();

//   const Real teps = (t_new[lev] - t_old[lev]) * 1.e-3;

//   if (time > t_new[lev] - teps && time < t_new[lev] + teps)
//     {
//       for (int n = 0; n < number_of_fabs; n++) data[n].push_back(phi_new[n][lev].get());
//       datatime.push_back(t_new[lev]);
//     }
//   else if (time > t_old[lev] - teps && time < t_old[lev] + teps)
//     {
//       for (int n = 0; n < number_of_fabs; n++) data[n].push_back(phi_old[n][lev].get());
//       datatime.push_back(t_old[lev]);
//     }
//   else
//     {
//       for (int n = 0; n < number_of_fabs; n++)
// 	{
// 	  data[n].push_back(phi_old[n][lev].get());
// 	  data[n].push_back(phi_new[n][lev].get());
// 	}
//       datatime.push_back(t_old[lev]);
//       datatime.push_back(t_new[lev]);
//     }
// }
