#ifndef __MY_HUFFMAN_HPP__
#define __MY_HUFFMAN_HPP__

#include <iostream>
#include <queue>
#include <stack>
#include <vector>
#include <memory>
#include <cstdint>
#include <cstring>
#include <arpa/inet.h>

namespace my_huffman
{
    /** huffman Tree Node */
    struct huffman_node
    {
        /** Data for each node. positive for leaf node, negative for intermediate node */
        int32_t data;
        /** Node weight */
        int weight;
        /** Left child pointer */
        std::shared_ptr<huffman_node> left;
        /** Right child pointer */
        std::shared_ptr<huffman_node> right;

        /** Constructor */
        huffman_node(int data, int weight, huffman_node *left = NULL, huffman_node *right = NULL);

        /** Comparer (for node weight) */
        bool operator<(const huffman_node &rhs) const;

        /** Comparer (for node weight) */
        bool operator>(const huffman_node &rhs) const;

        /** if two node have equal weight, compare data value */
        inline bool is_data_bigger_then(const huffman_node &rhs) const;
    };

    /** Base class for huffman Encode and Decode */
    class huffman
    {
    protected:
        /** huffman tree root */
        std::shared_ptr<huffman_node> _root;
        /** Input stream */
        std::istream *_input;

        /** Turn huffman tree to char table */
        void _build_char_table();

        /** Turn huffman tree to char table, with recursion */
        void _build_char_table(std::shared_ptr<huffman_node> &current, std::vector<uint8_t> &code);

    public:
        /** Char to huffman code */
        std::vector< std::vector<uint8_t> > char_table;

        /** Constructor */
        huffman(std::istream &input);

        /** Dummy function for derived classes */
        void virtual build_huffman_tree() = 0;

        std::vector<uint32_t> get_header();
    };

    /** huffman encode */
    class huffman_encode : public huffman
    {
    private:
        uint8_t *_result;
        size_t _result_size;
        uint32_t _file_size;

    public:
        /** Constructor */
        huffman_encode(std::istream &input);

        ~huffman_encode();

        /** Build huffman tree from input stream */
        void virtual build_huffman_tree()
        {
            using namespace std;

            std::istream &input = *_input;

            /** Count occurrence of every char in input stream */
            int freq[256] = { 0 };

            /** Get char from input stream */
            int c = input.get();
            while (!input.eof()) {
                freq[c] += 1;
                c = input.get();
            }

            // Clean flags (such as 'eofbit') for 'tellg()' to function
            input.clear();
            // Since the stream has reached the end, the position is file size
            _file_size = static_cast<uint32_t>(input.tellg());

            /** A min heap queue */
            priority_queue< huffman_node, vector<huffman_node>, greater<huffman_node> > table;

            for (int i = 0; i < 256; ++i) {
                if (freq[i] != 0) {
                    table.push(huffman_node(i, freq[i]));
                }
            }

            // Priority queue guarantees that the top is the least
            // Pop the least two and merge them
            while (table.size() > 1) {
                huffman_node *n1 = new huffman_node(table.top());
                table.pop();

                huffman_node *n2 = new huffman_node(table.top());
                table.pop();

                // Only leaf node have positive data value
                table.push(
                    huffman_node(
                        n1->data < 0 ? n1->data : -(n1->data) - 1,
                        n1->weight + n2->weight,
                        n1,
                        n2
                    )
                );
            }

            _root.reset(new huffman_node(table.top()));
            table.pop();
        }

        /** Write encoded huffman code and header to output stream */
        int write(std::istream &input, uint8_t **dst, int *dstlen)
        {
            using namespace std;

            if (_result != NULL) {
                *dst = _result;
                *dstlen = static_cast<int>(_result_size);
                return 0;
            }

            vector<uint32_t> header = get_header();
            header.insert(header.begin(), htonl(_file_size));
            
            size_t header_size = header.size() * sizeof (uint32_t);

            _result = (uint8_t *) malloc(_file_size / 2 + header_size);
            if (_result == NULL) {
                *dst = NULL;
                *dstlen = 0;
                return -1;
            }
            _result_size = _file_size / 2 + header_size;
            memcpy(_result, &header.front(), header_size);
            memset(_result + header_size, 0, _result_size - header_size);

            unsigned int result_bytes = header_size;
            unsigned int result_bit_offset = 0;

            int c = input.get();
            while (!input.eof()) {
                auto code = char_table.at(c);

                for (auto bit : code) {
                    _result[result_bytes] |= ( bit << (result_bit_offset) );
                    result_bit_offset += 1;
                    if (result_bit_offset >= 8) {
                        result_bytes += 1;
                        result_bit_offset = 0;
                    }
                }

                if (result_bytes + 256 >= _result_size) {
                    uint8_t *new_result = (uint8_t *) realloc(_result, _result_size + 65536);
                    if (new_result == NULL) {
                        free(_result);
                        _result_size = 0;
                        _result = NULL;
                        *dst = NULL;
                        *dstlen = 0;
                        return -1;
                    }
                    memset(new_result + _result_size , 0, 65536);
                    _result = new_result;
                    _result_size += 65536;
                }

                c = input.get();
            }

            _result_size = (result_bit_offset > 0) ? result_bytes + 1 : result_bytes;
            uint8_t *new_result = (uint8_t *) realloc(_result, _result_size);
            if (new_result == NULL) {
                free(_result);
                _result_size = 0;
                _result = NULL;
                *dst = NULL;
                *dstlen = 0;
                return -1;
            }

            _result = new_result;
            *dst = _result;
            *dstlen = static_cast<int>(_result_size);

            return 0;
        }

    };

    /** huffman decode */
    class huffman_decode : public huffman
    {
    private:
        std::streampos data_start;
        uint32_t _original_size;

    public:
        /** Constructor */
        huffman_decode(std::istream &input);

        ~huffman_decode();

        /** Build huffman tree from input stream */
        void virtual build_huffman_tree()
        {
            using namespace std;

            std::istream &input = *_input;

            input.read(reinterpret_cast<char *>(&_original_size), sizeof (_original_size));
            _original_size = ntohl(_original_size);

            /** Read the tree saved in input stream */
            uint32_t u_node_data;
            input.read(reinterpret_cast<char *>(&u_node_data), sizeof (u_node_data));
            u_node_data = ntohl(u_node_data);
            int32_t node_data = *(reinterpret_cast<int32_t *>(&u_node_data));

            _root.reset(new huffman_node(node_data, 0));

            // In case the root might not have children (i.e. root is leaf node)
            if (node_data < 0) {
                /** For restore tree from post order, without recursion */
                stack< shared_ptr<huffman_node> > huff_tree;
                huff_tree.push(_root);

                while (!huff_tree.empty()) {
                    /** Current tree node */
                    shared_ptr<huffman_node> current = huff_tree.top();

                    input.read(reinterpret_cast<char *>(&u_node_data), sizeof (u_node_data));
                    u_node_data = ntohl(u_node_data);
                    node_data = *(reinterpret_cast<int32_t *>(&u_node_data));

                    shared_ptr<huffman_node> new_node(new huffman_node(node_data, 0));

                    // Left child first (post order)
                    if (current->left == NULL) {
                        current->left = new_node;
                    }
                    else {
                        current->right = new_node;
                        // Both children are fulfilled, remove this node
                        huff_tree.pop();
                    }

                    // Not leaf node
                    if (node_data < 0) {
                        huff_tree.push(new_node);
                    }
                }
            }

            data_start = input.tellg();
            _build_char_table();
        }

        int write(std::ostream &output)
        {
            using namespace std;

            std::istream &input = *_input;

            input.clear();
            input.seekg(data_start);
            
            shared_ptr<huffman_node> current = _root;
            unsigned int result_bytes = 0;
            unsigned int input_bit_offset = 0;
            uint8_t buf[1024];
            streamsize buflen;
            streamsize pos = 0;

            input.read(reinterpret_cast<char *>(&buf), sizeof (buf));
            buflen = input.gcount();

            while (result_bytes < _original_size) {
                // If current bit is 1, go right
                // Magic, don't touch
                if ( buf[pos] & (1 << input_bit_offset) ) {
                    current = current->right;
                }
                else {
                    current = current->left;
                }

                if (current == NULL) {
                    return -1;
                }

                // Write char if reached leaf node
                if (current->data >= 0) {
                    output.put(static_cast<char>(current->data));
                    current = _root;
                    result_bytes += 1;
                }

                input_bit_offset += 1;
                if (input_bit_offset >= 8) {
                    input_bit_offset = 0;
                    pos += 1;
                }

                if (pos == buflen) {
                    if (input.eof()) {
                        return -1;
                    }
                    input.read(reinterpret_cast<char *>(&buf), sizeof (buf));
                    buflen = input.gcount();
                    pos = 0;
                }
            }

            return 0;
        }

    };
};

#endif
