#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <vector>
#include <cassert>

int main(int argc, char*argv[]) {

	typedef std::map<char, double> FreqMap;
	std::vector<char> cText, cText2;
	char c;
	std::string alphabet = "abcdefghijklmnopqrstuvwxyz";
	std::string key = "mlvdrxopwzqhykjsfuibnecatg";
	int shift = 11;
	FreqMap FMap, FMap2;

	std::cout << "Generating Frequency Analysis Chart For The File!" << std::endl;
	std::ifstream fin("Subsitution.txt");
	std::ifstream fin2("Shift.txt");

	//Opening the First File for Subsitution Cipher
	if (!fin.is_open()) {
		std::cout << "ERROR 404...Unable To Open File!";
	}
	int count = 0;
	while (fin.get(c)) {
		if ( ispunct(c)) continue;
		cText.push_back(c);
		if (isspace(c)) continue;
		FMap[c]++;
		count++;
	}
	//Opening the Second File for Shift Cipher
	if (!fin2.is_open()) {
		std::cout << "ERROR 404...Unable To Open File!";
	}
	int counts = 0;
	while (fin2.get(c)){
		if (ispunct(c) || isspace(c)) continue;
	cText2.push_back(c);
	FMap2[c]++;
	counts++;
	}

	//Printing Out The Frequency For The Subsitution Cipher Text
	std::cout << "Letter  Number  Percent %" << std::endl;
	fin.close();
	fin2.close();
	FreqMap::iterator Position;
	for (Position = FMap.begin(); Position != FMap.end(); ++Position) {
		std::cout << Position->first << "       " << (Position->second) << "       " << Position->second / count  << std::endl;
	}
	std::cout << "Total Number of Characters: " << count << std::endl << std::endl;
	//Decrypting The Cipher Text To Plain Text
	for (int i = 0; i < cText.size(); i++) {
		for (int j = 0; j < 26; j++) {
			if (cText[i] == key[j]) {
				cText[i] = alphabet[j];
				break;
			}
		}
	}
	//Printing out Subsitution PlainText
	for (int i = 0; i < cText.size(); i++)
		std::cout << cText[i];
	std::cout << std::endl << std::endl;
	
	//Decrypting The Shift Cipher Text to Plain Text
	for (int i = 0; i < cText2.size(); i++) {
		for (int j = 0; j < shift; j++) {
			if (cText2[i] == 'z') {
				cText2[i] = 'a';
			}
			else
				cText2[i]++;
		}
	}
	//Printing Out The Frequency Of the Shift Cipher Text
	std::cout << "Letter  Number  Percent %" << std::endl;
	for (Position = FMap2.begin(); Position != FMap2.end(); ++Position) {
		std::cout << Position->first << "       " << (Position->second) << "       " << Position->second / counts << std::endl;
	}
	std::cout << "Total Number of Characters: " << counts << std::endl << std::endl;

	std::cout << "The Results of The Shift Cipher." << std::endl;
	//Printing Out The Plain Text Of The Shift Cipher
	for (int i = 0; i < cText2.size(); i++)
		std::cout << cText2[i];
	std::cout << std::endl;
	return 0;
}
