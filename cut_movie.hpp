/*
 * Created by Ryan Jay 30.10.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace std;

typedef struct {
    int s;
    int e;
} Cut;

typedef vector<vector<float> > ScoreList;
typedef vector<Cut> CutList;


void cut( ScoreList score_list, string movie_file, vector<int> target_list, string output_dir="", 
        string temp_dir="/tmp", int total_targets = 6, int min_cut=5, int max_gap=2, float threshold=0.5, 
            float min_coverage=0.4 );
