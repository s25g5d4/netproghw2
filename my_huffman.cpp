#include <iostream>
#include <fstream>
#include <queue>
#include <stack>
#include <vector>
#include <algorithm>
#include <string>
#include <cstring>
#include <functional>
#include <memory>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "my_huffman.hpp"

using namespace my_huffman;

huffman_node::huffman_node(int data, int weight, huffman_node *left, huffman_node *right)
: data(data), weight(weight), left(left), right(right)
{

}

/** Comparer (for node weight) */
bool huffman_node::operator<(const huffman_node &rhs) const
{
    if (this->weight < rhs.weight) {
        return true;
    }
    else if (this->weight == rhs.weight) {
        return !is_data_bigger_then(rhs);
    }
    else {
        return false;
    }
}

/** Comparer (for node weight) */
bool huffman_node::operator>(const huffman_node &rhs) const
{
    if (this->weight > rhs.weight) {
        return true;
    }
    else if (this->weight == rhs.weight) {
        return is_data_bigger_then(rhs);
    }
    else {
        return false;
    }
}

/** if two node have equal weight, compare data value */
inline bool huffman_node::is_data_bigger_then(const huffman_node &rhs) const
{
    int l_data = this->data < 0 ? -(this->data) : this->data;
    int r_data = rhs.data < 0 ? -(rhs.data) : rhs.data;

    return (l_data > r_data);
}

void huffman::_build_char_table()
{
    using namespace std;

    // If root is leaf node (i.e. only one kind of char in input file)
    if (_root->data > 0) {
        _char_table.at(_root->data).push_back(0);

        return;
    }
    /** Left child huffman code */
    vector<uint8_t> l_code;
    /** Right child huffman code */
    vector<uint8_t> r_code;

    l_code.push_back(0);
    r_code.push_back(1);

    _build_char_table(_root->left, l_code);
    _build_char_table(_root->right, r_code);
}

/** Turn huffman tree to char table, with recursion */
void huffman::_build_char_table(std::shared_ptr<huffman_node> &current, std::vector<uint8_t> &code)
{
    using namespace std;

    if (current->data >= 0) {
        _char_table.at(current->data) = code;

        return;
    }

    /** Left child huffman code */
    auto l_code = code;
    /** Right child huffman code */
    auto r_code = code;

    l_code.push_back(0);
    r_code.push_back(1);

    _build_char_table(current->left, l_code);
    _build_char_table(current->right, r_code);
}

/** Constructor */
huffman::huffman(std::istream &input)
: _input(&input)
{
    // for each char (0~255)
    _char_table.resize(256);
}

std::vector<uint32_t> huffman::get_header()
{
    using namespace std;
    
    stack< shared_ptr<huffman_node> > huff_tree;
    huff_tree.push(_root);
    vector<uint32_t> tree;

    while (!huff_tree.empty()) {
        /** next node in post order to write to output stream */
        shared_ptr<huffman_node> current = huff_tree.top();
        huff_tree.pop();

        tree.push_back( htonl( *(reinterpret_cast<uint32_t *>(&current->data)) ) );

        if (current->right != NULL) {
            huff_tree.push(current->right);
        }
        if (current->left != NULL) {
            huff_tree.push(current->left);
        }
    }

    return tree;
}

/** huffman encode */
huffman_encode::huffman_encode(std::istream &input)
: huffman(input), _result(NULL), _result_size(0)
{
    build_huffman_tree();
    _build_char_table();
}

huffman_encode::~huffman_encode()
{
    free(_result);
}

/** huffman decode */
huffman_decode::huffman_decode(std::istream &input)
: huffman(input)
{
    build_huffman_tree();
    _build_char_table();
}

huffman_decode::~huffman_decode()
{
    
}
