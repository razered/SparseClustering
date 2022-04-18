/**
 * @file spectra.cpp
 * @brief Algorithms to cluster spectral data efficiently
 * 
 * We implement bottom-up agglomerative clustering of spectra using cosine similarity. 
 * We expose two methods - one that does this naively and the other that employs two approximation heuristics to speed up the process.
 * The first heuristic is that we only attempt clustering two spectra if their pepmasses are close to each other. 
 * The second heuristic is that we only cluster spectra if they share some top peaks.
 * 
 * @author Aditya Bhagwat abhagwa2@cs.cmu.edu
 */

#include "spectra.cpp"
#include "cluster.h"
#include <unordered_set>
#include <unordered_map>
#include <set> 
#include <math.h>
#include <chrono>
#include <iostream>
#include <algorithm>

#define DEFAULT_PEPMASS_THRESHOLD 2.f
#define DEFAULT_PEAK_THRESHOLD 0.02f
#define DEFAULT_SIMILARITY_THRESHOLD 0.7f

bool inline passes_pepmass_test(const spectrum_t& a, const spectrum_t& b) {
    return fabs(a.pepmass - b.pepmass) < DEFAULT_PEPMASS_THRESHOLD; 
}

bool inline is_identical_peak(const peak_t& a, const peak_t& b) {
    return fabs(a - b) <  DEFAULT_PEAK_THRESHOLD;
}

float cosine_similarity(const spectrum_t& a, const spectrum_t& b) {
    int i=0, j=0;
    float score=0;
    float a_den=0, b_den=0;
    while (i < a.num_peaks && j < b.num_peaks) {
        if (is_identical_peak(a.peaks[i], b.peaks[j])) {
            score += (a.intensities[i] * b.intensities[j]);
            a_den += pow(a.intensities[i], 2);
            b_den += pow(b.intensities[j], 2);
            i += 1;
            j += 1;
        } else if (a.peaks[i] < b.peaks[j]) {
            a_den += pow(a.intensities[i], 2);
            i += 1;
        }
        else if (a.peaks[i] > b.peaks[j]) {
            b_den += pow(b.intensities[j], 2);
            j += 1;
        }
    }
    return score / sqrt(a_den * b_den);
}

bool inline is_similar(const spectrum_t& a, const spectrum_t& b) {
    return cosine_similarity(a, b) > DEFAULT_SIMILARITY_THRESHOLD;
}

/**
* @brief 
* @param
* @return
*/
std::vector<int> initialize_cluster(int sz) {
    std::vector<int> clusters(sz, 0);
    for (int i=0; i<sz; i++) {
        clusters[i] = i;
    }
    return clusters;
}

// gets start point of peak_bucket. e.g. 50.01 lies in (50.00, 50.02) so this should return (50.00)
std::string get_peak_bucket(peak_t peak) {
    char buffer[10];
    int round_peak = floorf(peak * 100);
    int nearest = ((int) (round_peak) / 2) * 2;
    snprintf(buffer, 10, "%.*f", 2, nearest/100.f);
    return buffer;
}

std::vector<int> get_common_peak_candidates(const spectrum_t& spectrum, std::unordered_map<std::string, std::vector<int>>& peak_buckets) {
    std::vector<int> candidates;
    int sz = std::min(5, (int)spectrum.num_peaks);
    for (int i=0; i<sz; i++) {
        std::string peak_str = get_peak_bucket(spectrum.peaks[i]);
        if (peak_buckets.find(peak_str) != peak_buckets.end()) {
            candidates.insert(candidates.end(), peak_buckets[peak_str].begin(), peak_buckets[peak_str].end());
        } 
    }

    std::set<int> unq(candidates.begin(), candidates.end());
    candidates.assign(unq.begin(), unq.end());
    return candidates;
}

void bucket_spectrum_peaks(std::unordered_map<std::string, std::vector<int>>& peak_buckets, const spectrum_t& spectrum, int idx) {
    int sz = std::min(5, (int)spectrum.num_peaks);
    for (int i=0; i<sz; i++) {
        std::string peak = get_peak_bucket(spectrum.peaks[i]);
        peak_buckets[peak].push_back(idx);
    }
}

void dbg_print_buckets(std::unordered_map<std::string, std::vector<int>>& peak_buckets) {
    for (auto& it: peak_buckets) {
        printf("key : %s\t", it.first.c_str());
        for (int v: it.second)
            printf("%d ", v);
        printf("\n");
    }
}

// also uses pepmass test
/**
* @brief 
* @param
* @return
*/
void cluster_spectra(std::vector<int>& clusters, const std::vector<spectrum_t>& spectra) {
    std::unordered_map<std::string, std::vector<int>> peak_buckets;
    for (int i=0; i<spectra.size(); i++) {
        bool new_cluster = true;
        std::vector<int> candidates = get_common_peak_candidates(spectra[i], peak_buckets);
        for (const int& candidate: candidates) {
            if (passes_pepmass_test(spectra[i], spectra[candidate]) && is_similar(spectra[i], spectra[candidate])) {
                clusters[i] = candidate;
                new_cluster = false;
                break;
            } 
        }

        if (new_cluster) {
            bucket_spectrum_peaks(peak_buckets, spectra[i], i);
        }
    }
}

void print_clusters(const vector<int>& clusters) {

}

// includes the pepmass test
/**
* @brief 
* @param
* @return
*/
void naive_cluster_spectra(std::vector<int>& clusters, const std::vector<spectrum_t>& spectra) {
    for (int i=1; i<spectra.size(); i++) {
        std::unordered_set<int> seen_candidates;
        for (int j=0; j<i; j++) {
            int candidate = clusters[j];
            if (seen_candidates.find(candidate) != seen_candidates.end()) continue;
            if (passes_pepmass_test(spectra[i], spectra[candidate]) && is_similar(spectra[i], spectra[candidate])) {
                clusters[i] = candidate;
                break;
            } else {
                seen_candidates.insert(candidate);
            }
        }
    }
}

/**
* @brief 
* @param
* @return
*/
int main(int argc, char* argv[]) {
    using namespace std::chrono;
    typedef std::chrono::high_resolution_clock Clock;
    typedef std::chrono::duration<double> dsec;
    std::string file_path = "data/100000.mgf";
    printf("Parsing file %s ...\n", file_path.c_str());
    auto init_start = Clock::now();
    std::vector<spectrum_t> spectra = parseMgfFile(file_path);
    int sz = spectra.size();
    auto parsing_complete = Clock::now();
    printf("Parsing took %lf seconds\n", duration_cast<dsec>(parsing_complete - init_start).count());
    printf("Clustering %lu spectra ...\n", spectra.size());
    std::vector<int> clusters = initialize_cluster(sz); // stores the representative for the cluster that the ith spectrum belongs to
    cluster_spectra(clusters, spectra);
    auto clustering_complete = Clock::now();
    printf("Clustering took %lf seconds\n", duration_cast<dsec>(clustering_complete - parsing_complete).count());
    auto num_clusters = std::set<int>(clusters.begin(), clusters.end()).size();
    printf("The %lu spectra could be clustered into %lu clusters\n", spectra.size(), num_clusters);
}