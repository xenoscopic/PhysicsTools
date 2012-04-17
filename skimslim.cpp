//Standard includes
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

//Boost includes
#include <boost/program_options.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/algorithm/string.hpp>

//ROOT includes
#include <TError.h>
#include <TFile.h>
#include <TChain.h>
#include <TTree.h>
#include <TTreeFormula.h>

//Standard namespaces
using namespace std;

//Boost namespaces
using namespace boost;

//Boost namespace aliases
namespace po = boost::program_options;

po::variables_map parse_command_line_options(int argc, char * argv[])
{
	//Create the options specifier.  (This code is correct, it's
	//Boost voodoo using Boost.Assign.)
	po::options_description desc("Allowed Options");
	desc.add_options()
		("verbose,v", "Make the program print more detailed output to command line.")
		("input,i", po::value<string>()->required(), "The path (either a file or directory) to the input data.")
		("container,c", po::value<string>()->required(), "The name of the TTree container in the input file.")
		("selection,s", po::value< vector<string> >()->multitoken(), "A selection expression to apply to the tree.")
		("selection-file,S", po::value< vector<string> >()->multitoken(), "A file containing a selection expression to apply to the tree."
																			"  The file can contain multiple lines, each of which represents"
																			" a selection expression to apply.  Comments can be included if"
																			" you begin the line with #.")
		("enable-branches,e", po::value< vector<string> >()->multitoken(), "Enable a branch (overrides branch disabling).")
		("disable-branches,d", po::value< vector<string> >()->multitoken(), "Disable a branch.")
		("disable-all-branches,D", "Disable all branches.")
		("output,o", po::value<string>()->default_value("output.root"), "The output name for the ROOT data file.")
		("replace,r", "Replace the output file if it already exists.")
		("help,h", "Print a description of the program options.")
	;
	
	//Do the actual parsing
	po::variables_map vm;		

	try 
	{
		po::store(po::command_line_parser(argc, argv).options(desc).run(), vm);
		po::notify(vm);
		
		//Print help if necessary
		if(vm.count("help"))
		{
			cout << desc << endl;
			exit(1);
		}
	}
	catch(std::exception& e)
	{
		cerr << "Couldn't parse command line options: " << e.what() << endl;
		cout << desc << endl;
		exit(1);
	}
	catch(...)
	{
		cerr << "Couldn't parse command line options, not sure why." << endl;
		cout << desc << endl;
		exit(1);
	}
	
	return vm;
}

void set_branches_from_options(po::variables_map options, TTree *tree)
{
	//Determine operating parameters
	bool verbose = (options.count("verbose") > 0);

	//Enable all branches
	if(verbose)
	{
		cout << "Enabling all branches" << endl;
	}
	tree->SetBranchStatus("*", 1);

	//Disable all branches, if requested
	if(options.count("disable-all-branches") > 0)
	{
		if(verbose)
		{
			cout << "Disabling all branches" << endl;
		}
		tree->SetBranchStatus("*", 0);
	}

	//Disable any individually-requested branches
	if(options.count("disable-branches") > 0)
	{
		vector<string> disabled_branches = options["disable-branches"].as< vector<string> >();
		vector<string>::iterator disabled_branch;
		for(disabled_branch = disabled_branches.begin();
			disabled_branch != disabled_branches.end();
			disabled_branch++)
		{
			if(verbose)
			{
				cout << "Disabling branch(es): " << *disabled_branch << endl;
			}
			tree->SetBranchStatus(disabled_branch->c_str(), 0);
		}
	}

	//Enable any individually-requested branches
	if(options.count("enable-branches") > 0)
	{
		vector<string> enabled_branches = options["enable-branches"].as< vector<string> >();
		vector<string>::iterator enabled_branch;
		for(enabled_branch = enabled_branches.begin();
			enabled_branch != enabled_branches.end();
			enabled_branch++)
		{
			if(verbose)
			{
				cout << "Enabling branch(es): " << *enabled_branch << endl;
			}
			tree->SetBranchStatus(enabled_branch->c_str(), 1);
		}
	}
}

TTreeFormula * create_selection_formula_from_options(po::variables_map options, TTree *tree)
{
	//Determine operating parameters
	bool verbose = (options.count("verbose") > 0);

	//Create initial selection
	string total_selection = "1";

	//Loop over command line selection expressions
	if(options.count("selection") > 0)
	{
		vector<string> selections = options["selection"].as< vector<string> >();
		vector<string>::iterator selection;
		for(selection = selections.begin();
			selection != selections.end();
			selection++)
		{
			if(verbose)
			{
				cout << "Applying selection: " << *selection << endl;
			}
			total_selection += "*(" + *selection + ")";
		}
	}

	//Loop over selection files
	if(options.count("selection-file") > 0)
	{
		vector<string> selection_files = options["selection-file"].as< vector<string> >();
		vector<string>::iterator selection_file;
		for(selection_file = selection_files.begin();
			selection_file != selection_files.end();
			selection_file++)
		{
			//Open the file and make sure it opens correctly
			ifstream input_file(selection_file->c_str());
			if(!input_file.is_open())
			{
				cerr << "Could not load selection from path: " << *selection_file << endl;
				return NULL;
			}

			//Print information
			if(verbose)
			{
				cout << "Applying selection from file: " << *selection_file << endl;
			}

			//Go through and read each line
			string line;
			while(getline(input_file, line))
			{
				//Trim whitespace
				trim(line);

				//Check for empty lines or comments
				if(line.length() == 0 || line[0] == '#')
				{
					continue;
				}

				//Add the selection
				if(verbose)
				{
					cout << "\t" << line << endl;
				}

				total_selection += "*(" + line + ")";
			}

			//Close the input file
			input_file.close();
		}
	}

	//Create the result
	TTreeFormula *result = new TTreeFormula("selection", total_selection.c_str(), tree);

	//Check that it compiled correctly (hopefully
	//this check will continue to work)
	if(result->GetTree() == NULL)
	{
		cerr << "ERROR: Selection did not compile correctly." << endl;
		delete result;
		return NULL;
	}

	return result;
}

int main(int argc, char * argv[])
{
	//Parse command line options.  This will do all error detection.
	po::variables_map options = parse_command_line_options(argc, argv);

	//Determine operating parameters
	bool verbose = (options.count("verbose") > 0);
	bool replace = (options.count("replace") > 0);
	string input = options["input"].as<string>();
	string container = options["container"].as<string>();
	string output = options["output"].as<string>();
	
	//Print program information
	if(verbose)
	{
		cout << "Physics tools skim/slim script" << endl;
		cout << "Input file: " << input << endl;
		cout << "Input container: " << container << endl;
		cout << "Output file: " << output << endl;
	}
	else
	{
		//Disable ROOT program output (well, only 
		//print those things which are a break or 
		//worse.)
		gErrorIgnoreLevel = kBreak;
	}

	//Create the input tree (which may be a chain of trees)
	TChain *old_tree = new TChain(container.c_str());

	//Add the input paths
	old_tree->Add(input.c_str());

	//Create the output file
	string output_options = replace ? "RECREATE" : "CREATE";
	TFile *output_file = TFile::Open(output.c_str(), output_options.c_str());
	if(output_file == NULL)
	{
		//Unable to open the file
		cerr << "ERROR: Unable to open the output file for writing." << endl;

		//Clean up and exit
		delete old_tree;
		return 1;
	}
	output_file->cd();

	//Set branch status so we know what to read/include in the new file
	set_branches_from_options(options, old_tree);
	
	//Clone the tree (but don't copy any entries yet).
	//We are implicitly within the context of the new
	//file (this is just how ROOT operates), so this
	//new tree will automatically be added to that 
	//file.
	TTree *new_tree = old_tree->CloneTree(0);

	//Create the evaluation formula
	TTreeFormula *selector = create_selection_formula_from_options(options, old_tree);
	if(selector == NULL)
	{
		//Unable to compile selection formula
		cerr << "ERROR: Unable to create selection formula." << endl;

		//Clean up and exit
		output_file->Close();
		delete output_file;
		delete old_tree;
		return 1;
	}

	//HACK: Call SetNotify for the old_tree.  This is
	//only necessary because we are using a TChain.
	old_tree->SetNotify(selector);

	//Figure out how many entries there are
	Long64_t n_events = old_tree->GetEntries();
	if(verbose)
	{
		cout << "There are " << n_events << " entries." << endl;
	}

	//Loop over the entries
	for(Long64_t i = 0; i < n_events; i++)
	{
		//Set the entry in the old_tree (this doesn't 
		//load any data just yet).  TTreeFormula will
		//read what it needs.
		old_tree->LoadTree(i);

		//See if this entry is selected, and if so,
		//add it to the output tree.
		if(selector->EvalInstance(0))
		{
			old_tree->GetEntry(i);
			new_tree->Fill();
		}
	}

	//Save the output file (required for data
	//to be written) and close
	output_file->Write();
	output_file->Close();

	//Clean up.  No need to delete new tree,
	//it will be owned by the file.
	delete selector;
	delete output_file;
	delete old_tree;

	return 0;
}
