/*
 * Created by Ryan Jay 30.10.16
 * Covered by the GPL. v3 (see included LICENSE)
 */

#include <caffe/caffe.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <algorithm>
#include <iosfwd>
#include <memory>
#include <string>
#include <sstream>
#include <utility>
#include <vector>
#include <stdlib.h>
#include <unistd.h>
#include <dirent.h>
#include <fstream>
#include <boost/thread.hpp>
#include "cut_movie.hpp"
#include "util.hpp"


using namespace caffe;  // NOLINT(build/namespaces)
using namespace std;
using std::string;

int global_ffmpeg_done = -1;


class Classifier 
{
 public:
  Classifier(const string& model_file,
             const string& trained_file,
             const string& mean_file,
             const string& label_file);

  ScoreList  Classify(const vector<cv::Mat>& imgs);

  std::vector<string> labels_;

 private:
  void SetMean(const string& mean_file);

  std::vector<vector<float> > Predict(const vector<cv::Mat>& imgs);

  void WrapInputLayer(std::vector<cv::Mat>* input_channels, int n);

  void Preprocess(const cv::Mat& img,
                  std::vector<cv::Mat>* input_channels);

 private:
  boost::shared_ptr<Net<float> > net_;
  cv::Size input_geometry_;
  int num_channels_;
  cv::Mat mean_;
};

Classifier::Classifier(const string& model_file,
                       const string& trained_file,
                       const string& mean_file,
                       const string& label_file) 
{
#ifdef CPU_ONLY
  Caffe::set_mode(Caffe::CPU);
#else
  Caffe::set_mode(Caffe::GPU);
#endif


  /* Load the network. */
  net_.reset(new Net<float>(model_file, TEST));
  net_->CopyTrainedLayersFrom(trained_file);

  CHECK_EQ(net_->num_inputs(), 1) << "Network should have exactly one input.";
  CHECK_EQ(net_->num_outputs(), 1) << "Network should have exactly one output.";

  Blob<float>* input_layer = net_->input_blobs()[0];
  num_channels_ = input_layer->channels();
  CHECK(num_channels_ == 3 || num_channels_ == 1)
    << "Input layer should have 1 or 3 channels.";
  input_geometry_ = cv::Size(input_layer->width(), input_layer->height());

  /* Load the binaryproto mean file. */
  SetMean(mean_file);

  /* Load labels. */
  std::ifstream labels(label_file.c_str());
  CHECK(labels) << "Unable to open labels file " << label_file;
  string line;
  while (std::getline(labels, line))
    labels_.push_back(string(line));

  Blob<float>* output_layer = net_->output_blobs()[0];
  CHECK_EQ(labels_.size(), output_layer->channels())
    << "Number of labels is different from the output layer dimension.";
}


//Utility Functions

int IndexOf(string label, vector<string> labels)
{
    for( int k=0; k<labels.size(); k++)
        if(label == labels[k])
            return k;
    
    cerr << "Label not found in list: " << label << endl;
    exit(EXIT_FAILURE);
}

string FormatFileNumber(int file_no) 
{
    ostringstream out;
    out << std::internal << std::setfill('0') << std::setw(5) << file_no;
    return out.str();
}


inline bool FileExists(const std::string& name) 
{
    ifstream f(name.c_str());
    return f.good();
}

int CountFiles(string directory)
{
    DIR *dir;
    struct dirent *ent;
    int n=0;
    if ((dir = opendir (directory.c_str())) != NULL)
    {
        n = 0;
        while ((ent = readdir(dir)) != NULL) n++;
        closedir(dir);
        return n-2; //-2 for . and ..
    }
    else
    {
        cerr << "Could not open directory: " << directory << endl;
        exit(EXIT_FAILURE);
    }
}

void CreateScreenShots(string movie_file, string screenshot_directory)
{
  //turn movie into 1 second screenshots
  
  string mkdir_cmd = "mkdir -p " + screenshot_directory;
  if(system(mkdir_cmd.c_str()))
  {
      cerr << "Command failed: " << mkdir_cmd << endl;
      exit(EXIT_FAILURE);
  }

  string screenshot_cmd = "ffmpeg -loglevel 8 -i \"" + movie_file + "\" -vf fps=1 -q:v 1 " +
            screenshot_directory + "img_\%05d.jpg";
  if(system(screenshot_cmd.c_str()))
  {
      cerr << "Error getting screenshots from: " << movie_file << endl;
      exit(EXIT_FAILURE);
  }
  
  global_ffmpeg_done = CountFiles(screenshot_directory);

}

void PrintUsage(char* prog_name)
{
    cout << "Usage: " << prog_name << " [-t target|-x|-a] [-b batch_size] [-c] [-o output_dir] [options] movie_file" << endl;
    cout << "-h\tPrint more help information about options" << endl;
}

void PrintHelp()
{
    cout << endl;
    cout << "Main Options" << endl;
    cout << "-t\tComma separated list of the Targets to search for (default:blowjob_handjob)" << endl;
    cout << "-x\tRemove all non-sexual scenes. Same as all targets except \'other\'. Ignores -t." << endl;
    cout << "-a\tCreate a tag file with the cuts for all categories. Ignores -t and -x" << endl; 
    cout << "-b\tBatch size (default: 32) - decrease if you run out of memory" << endl;
    cout << "-c\tCPU-only (slower but doesn't require CUDA)" << endl;
    cout << "-o\tOutput directory (default: same as input)" << endl;
    cout << "-d\tTemporary Directory (default: /tmp)" << endl;
    cout << endl;
    cout << "Cutting Options" << endl;
    cout << "-u\tMinimum cUt in seconds (default: 4)" << endl;
    cout << "-g\tMax Gap (default: 2)- the largest section of non-target frames in a cut" << endl;
    cout << "-s\tMinimum Score (default: 0.5) - minimum value considered a match [0-1]" << endl;
    cout << "-v\tMinimum coVerage of target frames in a cut (default: 0.4) [0-1]" << endl;
    cout << endl;
    cout << "Model Options" << endl;
    cout << "-m\tMean file .binaryproto" << endl;
    cout << "-p\tDefinition of model .prototxt" << endl;
    cout << "-w\tWeights for model .caffemodel" << endl;
    cout << "-l\tLabel file" << endl;
}

vector<string> Split(const string &s, char delim) 
{
    stringstream ss(s);
    string item;
    vector<string> tokens;
    while (getline(ss, item, delim)) 
    {
        tokens.push_back(item);
    }
    return tokens;
}

vector<string> allExceptOther(vector<string> labels)
{
    vector<string> output;
    for(int i=0; i<labels.size(); i++)
    {
        if(labels[i] != "other")
        output.push_back(labels[i]);
    }
    return(output);
}


//Classifier Class Functions


/* Return the all predictions. */
ScoreList Classifier::Classify(const vector<cv::Mat>& imgs) 
{
  ScoreList outputs = Predict(imgs);
return outputs;
}

/* Load the mean file in binaryproto format. */
void Classifier::SetMean(const string& mean_file) 
{
  BlobProto blob_proto;
  ReadProtoFromBinaryFileOrDie(mean_file.c_str(), &blob_proto);

  /* Convert from BlobProto to Blob<float> */
  Blob<float> mean_blob;
  mean_blob.FromProto(blob_proto);
  CHECK_EQ(mean_blob.channels(), num_channels_)
    << "Number of channels of mean file doesn't match input layer.";

  /* The format of the mean file is planar 32-bit float BGR or grayscale. */
  std::vector<cv::Mat> channels;
  float* data = mean_blob.mutable_cpu_data();
  for (int i = 0; i < num_channels_; ++i) 
  {
    /* Extract an individual channel. */
    cv::Mat channel(mean_blob.height(), mean_blob.width(), CV_32FC1, data);
    channels.push_back(channel);
    data += mean_blob.height() * mean_blob.width();
  }

  /* Merge the separate channels into a single image. */
  cv::Mat mean;
  cv::merge(channels, mean);

  /* Compute the global mean pixel value and create a mean image
   * filled with this value. */
  cv::Scalar channel_mean = cv::mean(mean);
  mean_ = cv::Mat(input_geometry_, mean.type(), channel_mean);
}

std::vector<vector<float> > Classifier::Predict(const vector<cv::Mat>& imgs) 
{
  Blob<float>* input_layer = net_->input_blobs()[0];
  input_layer->Reshape(imgs.size(), num_channels_,
                       input_geometry_.height, input_geometry_.width);
  /* Forward dimension change to all layers. */
  net_->Reshape();

  for( int i=0; i < imgs.size(); ++i)
  {
      vector<cv::Mat> input_channels;
      WrapInputLayer(&input_channels,i);
      Preprocess(imgs[i], &input_channels);
  }

  net_->Forward();

  vector<vector<float> > outputs;

  Blob<float>* output_layer = net_->output_blobs()[0];
  for( int i=0; i < output_layer->num(); ++i)
  {
      const float* begin = output_layer->cpu_data() + i * output_layer->channels();
      const float* end = begin + output_layer->channels();
      /* Copy the output layer to a std::vector */
      outputs.push_back(vector<float>(begin, end));
  }
  return outputs;
}

/* Wrap the input layer of the network in separate cv::Mat objects
 * (one per channel). This way we save one memcpy operation and we
 * don't need to rely on cudaMemcpy2D. The last preprocessing
 * operation will write the separate channels directly to the input
 * layer. */
void Classifier::WrapInputLayer(std::vector<cv::Mat>* input_channels, int n) 
{
  Blob<float>* input_layer = net_->input_blobs()[0];

  int width = input_layer->width();
  int height = input_layer->height();
  int channels = input_layer->channels();
  float* input_data = input_layer->mutable_cpu_data() + n * width * height * channels;
  for (int i = 0; i < channels; ++i) 
  {
    cv::Mat channel(height, width, CV_32FC1, input_data);
    input_channels->push_back(channel);
    input_data += width * height;
  }
}

void Classifier::Preprocess(const cv::Mat& img,
                            std::vector<cv::Mat>* input_channels) 
{
  /* Convert the input image to the input image format of the network. */
  cv::Mat sample;
  if (img.channels() == 3 && num_channels_ == 1)
    cv::cvtColor(img, sample, cv::COLOR_BGR2GRAY);
  else if (img.channels() == 4 && num_channels_ == 1)
    cv::cvtColor(img, sample, cv::COLOR_BGRA2GRAY);
  else if (img.channels() == 4 && num_channels_ == 3)
    cv::cvtColor(img, sample, cv::COLOR_BGRA2BGR);
  else if (img.channels() == 1 && num_channels_ == 3)
    cv::cvtColor(img, sample, cv::COLOR_GRAY2BGR);
  else
    sample = img;

  cv::Mat sample_resized;
  if (sample.size() != input_geometry_)
    cv::resize(sample, sample_resized, input_geometry_);
  else
    sample_resized = sample;

  cv::Mat sample_float;
  if (num_channels_ == 3)
    sample_resized.convertTo(sample_float, CV_32FC3);
  else
    sample_resized.convertTo(sample_float, CV_32FC1);

  cv::Mat sample_normalized;
  cv::subtract(sample_float, mean_, sample_normalized);

  /* This operation will write the separate BGR planes directly to the
   * input layer of the network because it is wrapped by the cv::Mat
   * objects in input_channels. */
  cv::split(sample_normalized, *input_channels);

}


int main(int argc, char** argv) 
{
  
  int batch_size = 32;
  int MAX_IMG_IDX = 99999;
  int report_interval = 100;
  int sleep_time = 1;
  int min_cut = 4;
  int max_gap = 2;
  int min_score = 0.5;
  int min_coverage = 0.4;
  vector<string> target_list;
  target_list.push_back("blowjob_handjob");  //the default target
  string movie_file;
  string screenshot_directory = "/tmp/screenshots/";

  string model_dir = "model/";
  string model_weights = model_dir + "weights.caffemodel";
  string model_def = model_dir + "deploy.prototxt";
  string mean_file = model_dir + "mean.binaryproto";
  string label_file = model_dir + "labels.txt";
  string output_directory = "";
  string temp_directory = "/tmp";
  bool auto_tag = false;





  //parse command line flags
  int opt;
  bool set_all_but_other = false;
  while ((opt = getopt(argc, argv, "at:c:b:d:o:m:g:s:hxp:w:u:l:")) != -1) 
  {
        switch (opt) {
        case 'a':
            auto_tag = true;
            break;
        case 't':
            target_list = Split(optarg,','); 
            break;
        case 'b':
            batch_size = atoi(optarg);
            break;
        case 'd':
            temp_directory = optarg;
            break;
        case 'o':
            output_directory = optarg;
            break;
        case 'u':
            min_cut = atoi(optarg);
            break;
        case 'g':
            max_gap = atoi(optarg);
            break;
        case 's':
            min_score = atoi(optarg);
            break;
        case 'c':
            Caffe::set_mode(Caffe::CPU);
            break;
        case 'v':
            min_coverage = atoi(optarg);
            break;
        case 'm':
            mean_file = optarg;
            break;
        case 'p':
            model_def = optarg;
            break;
        case 'w':
            model_weights = optarg;
            break;
        case 'l':
            label_file = optarg;
            break;
        case 'h':
            PrintHelp();
            exit(0);
        case 'x':
            set_all_but_other = true;
            break;
        default: /* '?' */
            PrintUsage(argv[0]);
            exit(EXIT_FAILURE);
        }
  }

  if(optind >= argc)
  {
      cerr << "No input movie file." << endl;
      PrintUsage(argv[0]);
      exit(EXIT_FAILURE);
  }
  movie_file = argv[optind];


  //keep Caffe quiet
  FLAGS_minloglevel = 3;
  ::google::InitGoogleLogging(argv[0]);

  //create the classifier
  Classifier classifier(model_def, model_weights, mean_file, label_file);

  if(set_all_but_other)
        target_list = allExceptOther(classifier.labels_);

  //print targets
  if(auto_tag)
      cout << "Auto-tag mode" << endl;
  else
  {
      cout << "Targets: [";
      for(int i=0; i<target_list.size(); i++)
      {
          cout << target_list[i];
          if(i < target_list.size()-1)
              cout << ", ";
      }
      cout << "]" << endl;
  }

  
  global_ffmpeg_done = MAX_IMG_IDX;
  boost::thread first(CreateScreenShots, movie_file, screenshot_directory);
  //first.join();  //uncomment to make predictions wait for screenshots
    
  int epoch = 0;
  bool no_more = false;
  ScoreList score_list;

  //loop till all screenshots have been
  //extracted and classified
  while(true)
  {
    vector<cv::Mat> imgs;
    //fill a batch with screenshots to classify
    for( int i=0; i < batch_size; i++ )
    {
        
        int idx = epoch * batch_size + i + 1;

        //print some progress updates
        if(idx % report_interval == 0)
        {
            if(global_ffmpeg_done < MAX_IMG_IDX)
                cout << PrettyTime(idx) << "/" << PrettyTime(global_ffmpeg_done) << endl;
            else
                cout << PrettyTime(idx) << endl;
        }

        string the_image = "img_" + FormatFileNumber(idx) + ".jpg";
        string the_image_path = screenshot_directory + the_image;

        //wait for screenshots from ffmpeg thread
        while( !FileExists( the_image_path ) )
        {
            //if ffmpeg is done getting screenshots quit waiting
            if(idx >= global_ffmpeg_done)
            {
                no_more = true;
                break;
            }
            cout << " Waiting for: " + the_image_path << endl;
            sleep(sleep_time);
        }

        if(!no_more)
        {
            cv::Mat img = cv::imread(the_image_path,-1);
            CHECK(!img.empty()) << "Unable to decode image " << the_image_path;
            imgs.push_back(img);
        }
        else
            break;
    }

    //don't try to classify an empty batch
    if(imgs.size() == 0)
        break;

    //perform classification
    ScoreList ordered_preds = classifier.Classify(imgs);
    for( size_t i=0; i < ordered_preds.size(); ++i) 
        score_list.push_back(ordered_preds[i]);

    if(no_more)
        break;

    epoch += 1;
  }

  //Either create a file out the cuts for all targets
  //or make the cuts from the input list
  if(auto_tag)
  {
    TagTargets( score_list, movie_file, output_directory, classifier.labels_,
            classifier.labels_.size(), min_cut, max_gap, min_score ,min_coverage);
  }
  else
  {
    //make the cuts based on the predictions
    vector<int> target_ints;
    for(int i=0; i<target_list.size(); i++)
    {
      int target_idx = IndexOf(target_list[i],classifier.labels_);
      target_ints.push_back(target_idx);
    }
    CutMovie( score_list, movie_file, target_ints, output_directory, temp_directory, 
            classifier.labels_.size(), min_cut, max_gap, min_score, min_coverage );
  }

  //clean up screenshots
  string clean_cmd = "rm -rf " + screenshot_directory;
  if(system(clean_cmd.c_str()))
  {
    cerr << "Error cleaning up temporary files: " << clean_cmd << endl;
    exit(EXIT_FAILURE);
  }

}

