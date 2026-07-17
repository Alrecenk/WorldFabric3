#ifndef _LINEAR_PREDICTIVE_CODE_H_
#define _LINEAR_PREDICTIVE_CODE_H_ 1

#include "Utilities.h" // allow an unordered_map keyed on vec2
#include <vector> 
#include <map>

class LinearPredictiveCode{

const static inline double PI = 3.141592653589793 ;

public:

	int order = 0;
	int sample_rate = 0;
	int sample_size = 0;
	double signal_energy = 0;
	double residual_energy = 0;
	std::vector<double> code; // LPC code coefficients of a[0] - sum [ a[i] x^(-i)]  for i >= 1

	// Internal helper to compute autocorrelation R[0...p]
	static std::vector<double> computeAutocorrelation(std::vector<float>& signal, int order) {
		std::vector<double> R(order + 1, 0.0f);
		int N = (int)signal.size();

		for (int k = 0; k <= order; k++) {
			double sum = 0.0;
			for (int n = 0; n < N - k; n++) {
				sum += (double)signal[n] * (double)signal[n + k];
			}
			R[k] = sum;
		}
		return R;
	}

	// Solves the Toeplitz system for the coefficients using the Levinson-Durbin algorithm
	static std::vector<double> solveYuleWalker(const std::vector<double>& R) {
		int order = (int)(R.size() - 1) ;
		std::vector<double> a(order + 1, 0.0f);
		std::vector<double> e(order + 1, 0.0f);
		a[0] = 1.0 ;
		double error = R[0];
		e[0] = error;
		for (int k = 1; k <= order; k++) {
			double sum = 0.0;
			for (int j = 1; j < k; j++) {
				sum += a[j] * R[k - j];
			}
			double reflectionCoeff = (R[k] - sum) / e[k - 1];
			if (reflectionCoeff > 1) reflectionCoeff = 1;
			if (reflectionCoeff < -1) reflectionCoeff = -1;
			std::vector<double> a_old = a;
			for (int j = 1; j < k; j++) {
				a[j] = a_old[j] - reflectionCoeff * a_old[k - j];
			}
			a[k] = reflectionCoeff;
			e[k] = e[k - 1] * (1.0 - reflectionCoeff * reflectionCoeff);
			if(fabs(e[k]) < 1e-9){
				return a ; // we're no longer getting any error, so stop and leave the remaining a's = 0
			}
		}
		return a;
	}

	LinearPredictiveCode(std::vector<float>& signal, int sample_rate, int order){
		this->order = order ;
		this->sample_rate = sample_rate ;
		sample_size = (int)signal.size();
		std::vector<double> r = computeAutocorrelation(signal, order);
		code = solveYuleWalker(r);

		// Compute energies at creation time, so we don't need to hold onto the signal
		std::vector<float> residual(signal.size()) ;
		for (int k = 0; k < signal.size(); k++) {
			double prediction = 0.0f;
			for (int i = 1; i <= order; i++) {
				if (k - i >= 0) {
					prediction += code[i] * signal[k - i];
				}
			}
			residual[k] = (float)(signal[k] - prediction);
		}

		for (int k = 0; k < signal.size(); k++) {
			signal_energy += signal[k] * signal[k] ;
			residual_energy += residual[k] * residual[k];
		}
		signal_energy = sqrt(signal_energy/signal.size()) ;
		residual_energy = sqrt(residual_energy / signal.size());
	}
	
	static double getEnergy(std::vector<float> signal){
		double energy = 0 ;
		for (int k = 0; k < signal.size(); k++) {
			energy += signal[k] * (double)signal[k];;
		}
		energy = sqrt(energy / signal.size());
		return energy ;

	}


	//Applies a variety of preprocessing steps that might be helpful for audio
	static std::vector<float> prepareWindow(std::vector<float> raw, bool normalize = true, float pre_emphasis = 0.0f, bool hamming = false, float white_noise = 0.0f, int smooth_window_radius = 0) {

		for (auto& s : raw) {
			s += (randomFloat() - 0.5f) * white_noise;
		}

		std::vector<float> smoothed(raw.size());
		for (int k = 0; k < raw.size(); k++) {
			float sum = 0;

			int count = 0;
			for (int i = -smooth_window_radius; i <= smooth_window_radius; i++) {
				if (k + i >= 0 && k + i < raw.size()) {
					sum += raw[k + i];
					count++;
				}
			}
			smoothed[k] = sum / count;
		}
		raw = smoothed;

		if (normalize) {
			float total = 0;
			float max = -1e7f;
			float min = 1e7f;
			for (int i = 0; i < raw.size(); i++) {
				total += raw[i];
				max = fmax(raw[i], max);
				min = fmin(raw[i], min);
			}

			float mean = total / raw.size();
			//float scale = 0.95f / fmax(max - mean, mean - min);

			float scale = 1.0f /fmax(fabs(min), fabs(max)) ;
			for (int i = 0; i < raw.size(); i++) {
				//raw[i] -= mean;
				raw[i] *= scale;
			}
			//printf("mean : %f, min: %f, max: % f\n", mean, min, max);
		}

		std::vector<float> processed(raw.size());
		processed[0] = raw[0];
		for (int i = 1; i < raw.size(); i++) {
			processed[i] = raw[i] - (pre_emphasis * raw[i - 1]);
		}

		if (hamming) {
			for (size_t i = 0; i < raw.size(); i++) {
				float windowValue = 0.54f - 0.46f * (float)cos(2.0f * 3.141592653589793 * i / (raw.size() - 1));
				processed[i] *= windowValue;
			}
		}

		return processed;
	}


	std::vector<ComplexNumber> computeSoundPoles(){
		/* The LPC defines the polynomial 1 - sum[a_k ^ (-k)] for k >= 1
		* But the root solver is made for polynomials with positive exponents of the form p(x) = sum(a_k ^ k) for k >=0
		* To map between these we need to invert the signs of all but the highest order and reverse the order.
		* We're multiplying by x^order (which preserves the roots) but notably the LPC coefficients are 
		* subtracted and in descending order of exponent but we need 
		* coefficients that are added in ascending order of exponent for our polynomial.
		* This took days to figure out, so just trust me and don't touch it. - Alrecenk
		*/
		std::vector<double> flipped;
		for (int k = (int)code.size() - 1; k > 0; k--) {
			flipped.push_back(-code[k]);
		}
		flipped.push_back(1.0) ;
		Polynomial p(flipped);
		return p.findRoots() ;
	}

	
	// Generate a robotic voice mimicing the resonace of the original signal for verification
	std::vector<float> synthesizeVoice(float pitch_hz, float volume) {
		std::vector<float> final_robotic_signal(sample_size, 0.0f);
		std::vector<double> robotic_signal(sample_size, 0.0f);
		if(residual_energy > signal_energy){ // solution was unstable
			return final_robotic_signal ; // just return no sound as this was probably background noise
		}
	
		int period = static_cast<int>(sample_rate / pitch_hz);
		double total = 0 ;
		float min = 1e7f ;
		float max = -1e7f;
		for (int n = 0; n < sample_size; n++) {
			double synthesis = (n % period == 0) ? volume : 0.0f;
			for (int i = 1; i <= order; i++) {
				if (n - i >= 0) {
					synthesis += code[i] * robotic_signal[n - i];
				}
			}
			robotic_signal[n] = synthesis;
		
			//demphasis to counter emphasis added before computing LPC
			if(n == 0){
				final_robotic_signal[n] = (float)robotic_signal[0] ;
			}else{
				final_robotic_signal[n] = (float)(robotic_signal[n] + 0.97 * robotic_signal[n-1]) ;
				//final_robotic_signal[n] = (float)robotic_signal[n];
			}
			total += final_robotic_signal[n] ;
			if(final_robotic_signal[n] > max) max = final_robotic_signal[n] ;
			if (final_robotic_signal[n] < min) min = final_robotic_signal[n];
		}

		float mean = (float)(total/sample_size) ;
		float scale = 0.25f / fmax(max - mean, mean - min);
		for (int i = 0; i < final_robotic_signal.size(); i++) {
			final_robotic_signal[i] -= mean;
			final_robotic_signal[i] *= scale;
		}

		return final_robotic_signal;
	}

	//Returns the frequencies of the F1 and F2 formants from a set of complex roots extracted from an LPC
	static glm::vec2 extractFormantFrequencies(const std::vector<ComplexNumber>& roots, int sample_rate, double min_magnitude = 0.5, double max_magnitude = 1.2, double min_frequency = 100, double max_frequency = 8000) {
		glm::vec2 result  (1e7f,1e7f) ; // since we want lowest frequency, start with very high numbers
		for (const auto& root : roots) {
			if (root.imaginary > 0) { // Only look at one of each root pair
				double angle = std::atan2(root.imaginary, root.real);
				float frequency= (float)((angle * sample_rate) / (2.0 * PI));
				float magnitude = (float)sqrt(root.real * root.real + root.imaginary * root.imaginary);
				//printf("frequency: %f magnitude: %f\n", frequency, magnitude) ;
				//Filter for noisy, irrelevant, or unstable roots
				if (magnitude > min_magnitude && magnitude < max_magnitude && frequency > min_frequency && frequency < max_frequency) {
					//Keep the lowest two frequencies
					if(frequency < result.x){
						result.y = result.x ;
						result.x = frequency ;
					}else if(frequency < result.y){
						result.y = frequency ;
					}
				}
			}
		}
		return result ;

	}

	//Computes the relative distance between two frequencies in musical cents
	//Using logarithmic space distances produces a diference more closely related to how much difference a human would percieve
	static float centDistance(float frequency_1, float frequency_2, double reference = 200){
		double f1_cents = 1200.0 * std::log2(frequency_1 / reference);
		double f2_cents = 1200.0 * std::log2(frequency_2/ reference);
		return (float)(f1_cents - f2_cents) ;

	}
	static float centDistance(glm::vec2 formant_1, glm::vec2 formant_2, double reference = 200) {
		float dx = centDistance(formant_1.x, formant_2.x, reference) ;
		float dy = centDistance(formant_1.y, formant_2.y, reference);
		return sqrtf(dx*dx + dy*dy) ;

	}	

	//Generates a signal consisting of several addid pure frequencies each with the given amplitude
	static std::vector<float> generateWaveForm(int sample_rate, float duration, std::vector<float> frequency, float amplitude) {
		std::vector<float> wave((int)(duration * sample_rate), 0);
		for (int j = 0; j < wave.size(); j++) {
			double time = j / (double)sample_rate;
			for (int k = 0; k < frequency.size(); k++) {
				wave[j] += (float)(amplitude * sin(2.0 * PI * frequency[k] * time));
			}
		}
		return wave;
	}

	// Generates a signal that is a slightly noisy single-pole resonance
	static std::vector<float> generateSingleFormant(int sample_rate, float duration, float freq, float r) {
		std::vector<float> signal((int)(sample_rate*duration));
		double theta = 2.0 * PI * freq / sample_rate;
		double a1 = -2.0 * r * cos(theta);
		double a2 = r * r;

		for (int n = 0; n < signal.size(); n++) {
			float x = (randomFloat()-0.5f)*0.1f; // White noise excitation
			double prev1 = (n >= 1) ? signal[n - 1] : 0;
			double prev2 = (n >= 2) ? signal[n - 2] : 0;

			signal[n] = (float)(x - (a1 * prev1) - (a2 * prev2));
		}
		return signal;
	}

	//Generates a slightly noisy signal with two resonance poles
	static std::vector<float> generateDoubleFormant(int sample_rate, float duration,
		float f1, float r1,
		float f2, float r2, bool pulse = true, float pulse_frequency = 120.0f, int period_variance = 5 ) {
		std::vector<float> signal((int)(sample_rate * duration));
		double theta = 2.0 * PI * f1 / sample_rate;
		double a1 = -2.0 * r1 * cos(theta);
		double a2 = r1 * r1;
		int period = static_cast<int>(sample_rate / pulse_frequency);
		int next= 0 ;
		for (int n = 0; n < signal.size(); n++) {
			float x = (randomFloat() - 0.5f) * 0.01f; // White noise excitation
			if(pulse){
				x = (n == next) ? 0.01f : 0 ;
				next += period + (int)((randomFloat()-0.5f)*period_variance) ;
			}
			double prev1 = (n >= 1) ? signal[n - 1] : 0;
			double prev2 = (n >= 2) ? signal[n - 2] : 0;

			signal[n] = (float)(x - (a1 * prev1) - (a2 * prev2));
		}

		// We feed the output of stage 1 into stage 2
		std::vector<float> stage2(signal.size());
		double theta2 = 2.0 * PI * f2 / sample_rate;
		double a1_2 = -2.0 * r2 * cos(theta2);
		double a2_2 = r2 * r2;

		for (int n = 0; n < signal.size(); n++) {
			double p1 = (n >= 1) ? stage2[n - 1] : 0;
			double p2 = (n >= 2) ? stage2[n - 2] : 0;
			stage2[n] = (float)(signal[n] - (a1_2 * p1) - (a2_2 * p2));
		}

		return stage2;
	}

	// The LPC solver is an efficient method for solving a speciic system of equations (the Yule-Walker equations)
	// This function verifies that it works for a given autocorrelation result
	// By checking that the matrix equation is actually solved
	static void verifyLPCSolver(const std::vector<double>& R) {
		int order = (int)R.size() - 1;

		// Get the solver's answer for the given R
		std::vector<double> code = LinearPredictiveCode::solveYuleWalker(R);

		// The X should solve the system M*X = Y
		std::vector<double> X;
		for (int i = 1; i <= order; i++) {
			X.push_back(code[i]);
		}

		// M is the toeplitz matrix with R on the diagonals
		std::vector<std::vector<double>> M(order, std::vector<double>(order));
		for (int i = 0; i < order; i++) {
			for (int j = 0; j < order; j++) {
				// The value depends on the absolute distance from the diagonal
				M[i][j] = R[std::abs(i - j)];
			}
		}

		// and Y is also constructed from R
		std::vector<double> Y_expected(order);
		for (int i = 0; i < order; i++) {
			Y_expected[i] = R[i + 1];
		}

		// Calculate the Y from M*X with the computed X
		std::vector<double> Y_calculated(M.size()) ;
		for (int i = 0; i < M.size(); i++) {
			for (int j = 0; j < M.size(); j++) {
				Y_calculated[i] += M[i][j] * X[j];
			}
		}

		double max_error = 0;
		for (int i = 0; i < order; i++) {
			double diff = std::abs(Y_expected[i] - Y_calculated[i]);
			max_error = std::max(max_error, diff);
		}

		if (max_error < 1e-7) {
			printf("solveYuleWalker function is solving the YuleWalker equations.\n");
		}else {
			printf("solveYuleWalker FAILED - Difference found in toeplitz equations. Max Error: %f \n", max_error) ;
		}
	}


	static void testPipeline( float f1, float f2){
		int sample_rate = 48000;
		int order = 16 ;
		float duration  = 0.03f;
		//std::vector<float> signal = generateWaveForm(sample_rate,duration, {700, 1200, 2500}, 0.5f) ;
		//std::vector<float> signal = generateSingleFormant(sample_rate, duration, 700.0f, 0.98f) ;
		std::vector<float> signal = generateDoubleFormant(sample_rate, duration, f1, 0.98f, f2, 0.98f);
		signal = prepareWindow(signal,  true, 0, true, 0, 1) ;

		printf("Running audio formant pipeline test with a synthetic wave...\n") ;

		std::vector<double> R = computeAutocorrelation(signal, order);
		printf("Autocorrelation values: %f", (float) R[0]) ;
		for(int k=1;k<R.size();k++){
			printf(", %f", (float)R[k]) ;
		}
		printf("\n");

		std::vector<double> code = solveYuleWalker(R);
		printf("LPC code values: %f", (float)code[0]);
		for (int k = 1; k < code.size(); k++) {
			printf(", %f", (float)code[k]);
		}
		printf("\n");

		verifyLPCSolver(R);

		std::vector<float> residual(signal.size());
		for (int k = 0; k < signal.size(); k++) {
			double prediction = 0.0f;
			for (int i = 1; i <= order; i++) {
				if (k - i >= 0) {
					prediction += code[i] * signal[k - i];
				}
			}
			residual[k] = (float)(signal[k] - prediction);
		}
		double signal_energy = 0 ;
		double residual_energy = 0 ;
		for (int k = 0; k < signal.size(); k++) {
			signal_energy += signal[k] * signal[k];
			residual_energy += residual[k] * residual[k];
		}
		signal_energy = sqrt(signal_energy / signal.size());
		residual_energy = sqrt(residual_energy / signal.size());


		printf("Signal energy: %f >> residual energy: %f\n", signal_energy, residual_energy);

		std::vector<double> flipped;
		for (int k = (int)code.size() - 1; k > 0; k--) {
			flipped.push_back(-code[k]);
		}
		flipped.push_back(1.0);

		Polynomial p(flipped);

		
		std::vector<ComplexNumber> roots = p.findRoots();

		printf("Polynomial after multiplying by x^%d:\n", order) ;
		std::string p_string = p.toString();
		double total_root_error = 0 ;
		printf("%s\n", p_string.c_str());
		for (int k = 0; k < roots.size(); k++) {
			ComplexNumber value = p.apply(roots[k]);
			total_root_error += value.real*value.real + value.imaginary*value.imaginary ;
			printf("root %d : %f + %f * i  - >  P(root): %f + %f i \n", k, (float)roots[k].real, (float)roots[k].imaginary, (float)value.real, (float)value.imaginary);
		}
		if(total_root_error <= 1e-5f ){
			printf("Polynomial roots are correct (for the polynomial used)!\n");
		}else{
			printf("Polynomial root finding failed with error = %f .\n", total_root_error);
		}

		printf("Extracting formants:\n");
		glm::vec2 formants = extractFormantFrequencies(roots, sample_rate);

		printf("F1 expected: %f, got %f\n", f1, formants.x);
		printf("F2 expected: %f, got %f\n", f2, formants.y);
	}

	static void testPreprocessingParameters(int sample_rate, float signal_noise, int tries, float tolerance, float duration, int order, bool normalize, float pre_emphasis, bool hamming, float white_noise, int smooth_window_radius){

		std::vector<glm::vec2> relevant_formants = {
			{400, 2100}, {300,1400}, {300, 2200}, {360, 1900}, {220,1800}, {260, 1400}, {220, 2300}, {230,2400}, {550, 2700}, {900,1700}
		};

		//jiggle the test formants to prevent lucky resonance in the test
		for(auto& form : relevant_formants){
			form.x *= 1.0f + (randomFloat() - 0.5f)* 0.1f ;
			form.y *= 1.0f + (randomFloat() - 0.5f) * 0.1f;
		}

		int total = 0 ;
		int close = 0 ;
		glm::vec2 close_error ;
		float total_error = 0 ;
	

		for(int k=0;k<tries;k++){

				for(int j=0;j<relevant_formants.size();j++){
					glm::vec2 target = relevant_formants[j] ;
					std::vector<float> signal = generateDoubleFormant(sample_rate, duration, target.x, 0.97f, target.y, 0.97f);
					for(auto&f : signal){
						f+= (randomFloat()-0.5f)*signal_noise ;
					}
					signal = prepareWindow(signal, normalize, pre_emphasis, hamming, white_noise, smooth_window_radius);
					LinearPredictiveCode LPC(signal,sample_rate, order) ;
					std::vector<ComplexNumber> poles = LPC.computeSoundPoles();
					glm::vec2 result = extractFormantFrequencies(poles, sample_rate);
					//printf("%f, %f - > %f, %f\n" ,target.x, target.y, result.x, result.y) ;
					total++;
					glm::vec2 error = result-target ;
					if(fabs(error.x)/target.x < tolerance && fabs(error.y) / target.y < tolerance){
						close++;
						close_error += error ;
						total_error += fabs(error.x) + fabs(error.y) ;
					}
				}
			
		}	

		close_error /= total;
		total_error /= close;

		printf("%d/ %d close with average error : %f and shift: %f, %f\n", close, total, total_error, close_error.x, close_error.y) ;

	}


};




#endif // #ifndef _LINEAR_PREDICTIVE_CODE_H_
