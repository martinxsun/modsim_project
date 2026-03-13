#include "setup.hpp"


void main_setup() { // NACA 001034 wing tests; required extensions in defines.hpp: FP16S, EQUILIBRIUM_BOUNDARIES, SUBGRID, INTERACTIVE_GRAPHICS or GRAPHICS, FORCE_FIELD
	/// define simulation box size, viscosity and volume force ///
	const uint3 lbm_N = resolution(float3(2.0f, 3.0f, 1.5f), 7000u); // input: simulation box aspect ratio and VRAM occupation in MB, output: grid resolution
	
	const float si_u = 20.0f; // flow velocity [m/s]
	const float si_chord = 0.75f; // geometric mean chord (GMC) [m]
	const float si_span = 2.0f; // wingspan [m]
	const float si_area = 1.5f; // planform area [m^2]
	const float si_nu = 1.48E-5f; // air kinematic viscosity [m^2/s]
	const float si_rho = 1.225f; // air density [kg/m^3]

	const float lbm_u = 0.075f; // LBM velocity
	const float lbm_span = 0.85f * (float)lbm_N.x; // wingspan in LBM cells
	const ulong lbm_T = 30000ull; // LBM total time steps 
	units.set_m_kg_s(lbm_span, lbm_u, 1.0f, si_span, si_u, si_rho); // unit conversion
	const float lbm_nu = units.nu(si_nu);
	const float lbm_chord = units.x(si_chord); // GMC length in LBM cells
	
	const string wing_types[] = {"delta", "trap", "swept"}; // "delta", "trap", "swept"

	// loop thorugh wing types and angles of attack, run simulations and export data and images
	for (const string& wing_type : wing_types) {
		const string res_path = get_exe_path() + "export/" + wing_type + "/res.txt";
		write_file(res_path, "AoA\tCd\tCl\n");

		float aoa = 10.0f; // angle of attack [deg]
		const float aoa_lim = 60.0f;

		while (aoa <= aoa_lim) {
			// create lbm instance
			LBM lbm(lbm_N, lbm_nu);
			const uint Nx = lbm.get_Nx(), Ny = lbm.get_Ny(), Nz = lbm.get_Nz();
			const float3 center = float3(lbm.center().x, 1.3f * lbm_chord, lbm.center().z);

			// clear flags and voxelize
			parallel_for(lbm.get_N(), [&](ulong n) { lbm.flags[n] = 0u; });
			lbm.voxelize_stl(get_exe_path() + "../stl/naca001034_" + wing_type + ".stl", center, float3x3(float3(1, 0, 0), radians(-aoa)), lbm_span, TYPE_S);

			parallel_for(lbm.get_N(), [&](ulong n) { uint x = 0u, y = 0u, z = 0u; lbm.coordinates(n, x, y, z);
			if (lbm.flags[n] == TYPE_S) lbm.flags[n] = TYPE_S | TYPE_X;
			if (!(lbm.flags[n] & TYPE_S)) lbm.u.y[n] = lbm_u;
			if (x == 0u || x == Nx - 1u || y == 0u || y == Ny - 1u || z == 0u || z == Nz - 1u) lbm.flags[n] = TYPE_E; });

			// print info
			print_info("AoA = " + to_string(aoa, 1u) + " deg");
			print_info("Re = " + to_string(to_uint(units.si_Re(si_chord, si_u, si_nu))));
			print_info("Geometric mean chord = " + to_string(to_uint(lbm_chord)) + " cells");

			/// run simulation, export images and data ///
			const ulong avg_window = 5000ull; // average Cd and Cl over this many time steps
			double Cd_sum = 0.0, Cl_sum = 0.0;
			ulong avg_count = 0ull;
			const float q = 0.5f * si_rho * sq(si_u) * si_area;

#if defined(GRAPHICS) && !defined(INTERACTIVE_GRAPHICS)
			lbm.run(0u, lbm_T); // initialize simulation

			while (lbm.get_t() < lbm_T) { // main simulation loop
				// compute forces
				const float3 lbm_force = lbm.object_force(TYPE_S | TYPE_X);
				const float Cd = units.si_F(lbm_force.y) / q;
				const float Cl = units.si_F(lbm_force.z) / q;

				if (lbm.get_t() >= lbm_T - avg_window) {
					Cd_sum += (double)Cd;
					Cl_sum += (double)Cl;
					avg_count++;
				}

				// visualizations and prints
				if (lbm.graphics.next_frame(lbm_T, 2.0f) && lbm.get_t() >= lbm_T - avg_window) {
					print_info("t=" + to_string(lbm.get_t()) + " Cd=" + to_string(Cd, 4u) + " Cl=" + to_string(Cl, 4u));
					
					lbm.graphics.slice_mode = 1;
					lbm.graphics.slice_x = Nx / 3;
					lbm.graphics.set_camera_centered(0.0f, 0.0f, 30.0f, 1.6f);

					lbm.graphics.visualization_modes = VIS_FIELD;
					lbm.graphics.write_frame(get_exe_path() + "export/" + wing_type + "/" + to_string(aoa, 1u) + "/side_field/");

					lbm.graphics.visualization_modes = VIS_STREAMLINES;
					lbm.graphics.write_frame(get_exe_path() + "export/" + wing_type + "/" + to_string(aoa, 1u) + "/side_streamlines/");

					lbm.graphics.visualization_modes = VIS_FLAG_SURFACE | VIS_Q_CRITERION;
					lbm.graphics.slice_mode = 0;
					lbm.graphics.set_camera_free(float3(1.0f * (float)Nx, 0.68f * (float)Ny, 0.50f * (float)Nz), 44.0f, 15.0f, 62.0f);
					lbm.graphics.write_frame(get_exe_path() + "export/" + wing_type + "/" + to_string(aoa, 1u) + "/3d/");
				}

				lbm.run(1u, lbm_T);
			}
#else // GRAPHICS && !INTERACTIVE_GRAPHICS
			lbm.run();
#endif // GRAPHICS && !INTERACTIVE_GRAPHICS

			const float Cd_avg = (float)(Cd_sum / (double)avg_count);
			const float Cl_avg = (float)(Cl_sum / (double)avg_count);
			print_info("AoA=" + to_string(aoa, 1u) + " Cd_avg1=" + to_string(Cd_avg, 6u) + " Cl_avg1=" + to_string(Cl_avg, 6u) + " (" + to_string(avg_count) + " samples)");
			write_line(res_path, to_string(aoa, 1u) + "\t" + to_string(Cd_avg, 6u) + "\t" + to_string(Cl_avg, 6u) + "\n");

			aoa += 10.0f;
		}
	}
}