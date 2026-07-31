int operation(double first, double second, double *result){
    if (result == 0 || second == 0.0) {
        return 0;
    }

    *result = first / second;

    return 1;
}
