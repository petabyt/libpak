import js from "@eslint/js";

export default [
  // Use the recommended rules for all .js files
  {
    files: ["**/*.js"],
    rules: {
      ...js.configs.recommended.rules,
      // You can add your own custom rules here
      "no-unused-vars": "warn",
      "no-console": "off",
    },
  },
];
