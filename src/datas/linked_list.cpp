#include <cstddef>
#include <iostream>
#include "vector.cpp"

#ifndef CHESS_LINKED_LIST_H
#define CHESS_LINKED_LIST_H

class linked_list {
private:
	struct Node {
		vector data;
		Node* next;

		Node(const vector& value, Node* next_node = nullptr)
			: data(value), next(next_node) {}
	};

	Node* head = nullptr;
	Node* tail = nullptr;
	std::size_t list_size = 0;

public:
	linked_list() = default;

	~linked_list() {
		clear();
	}

	linked_list(const linked_list&) = delete;
	linked_list& operator=(const linked_list&) = delete;

	void push_front(const vector& move) {
		head = new Node(move, head);

		if (tail == nullptr) {
			tail = head;
		}

		++list_size;
	}

	void push_back(const vector& move) {
		Node* new_node = new Node(move);

		if (tail == nullptr) {
			head = tail = new_node;
		} else {
			tail->next = new_node;
			tail = new_node;
		}

		++list_size;
	}

	bool remove(const vector& move) {
		Node* previous = nullptr;
		Node* current = head;

		while (current != nullptr &&
				(current->data.x != move.x || current->data.y != move.y)) {
			previous = current;
			current = current->next;
		}

		if (current == nullptr) {
			return false;
		}

		if (previous == nullptr) {
			head = current->next;
		} else {
			previous->next = current->next;
		}

		if (current == tail) {
			tail = previous;
		}

		delete current;
		--list_size;
		return true;
	}

	bool contains(const vector& move) const {
		for (Node* current = head; current != nullptr; current = current->next) {
			if (current->data.x == move.x && current->data.y == move.y) {
				return true;
			}
		}

		return false;
	}

	void print() const {
		for (Node* current = head; current != nullptr; current = current->next) {
			std::cout << "[" << current->data.x << ", "
				      << current->data.y << "] -> ";
		}

		std::cout << "nullptr\n";
	}

	std::size_t size() const {
		return list_size;
	}

	bool empty() const {
		return head == nullptr;
	}

	void clear() {
		while (head != nullptr) {
			Node* next = head->next;
			delete head;
			head = next;
		}

		tail = nullptr;
		list_size = 0;
	}
};

#endif